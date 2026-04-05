#include "N1MCPServerRunnable.h"
#include "N1UnrealMCPModule.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

// ═══════════════════════════════════════════════════════════════
// FN1MCPClientRunnable — 개별 클라이언트 세션 스레드
// ═══════════════════════════════════════════════════════════════

FN1MCPClientRunnable::FN1MCPClientRunnable(FSocket* InSocket, FOnMCPCommandReceived InOnCommand, int32 InClientId)
	: ClientSocket(InSocket)
	, OnCommandReceived(MoveTemp(InOnCommand))
	, bShouldRun(true)
	, bFinished(false)
	, ClientId(InClientId)
{
}

FN1MCPClientRunnable::~FN1MCPClientRunnable()
{
}

void FN1MCPClientRunnable::Stop()
{
	bShouldRun = false;
}

uint32 FN1MCPClientRunnable::Run()
{
	ClientSocket->SetNonBlocking(false);

	FString MessageBuffer;
	uint8 RecvBuffer[4096];

	while (bShouldRun)
	{
		int32 BytesRead = 0;
		if (!ClientSocket->Recv(RecvBuffer, sizeof(RecvBuffer) - 1, BytesRead))
		{
			break;
		}

		if (BytesRead <= 0)
		{
			ESocketErrors LastError = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();
			if (LastError == SE_EWOULDBLOCK)
			{
				FPlatformProcess::Sleep(0.01f);
				continue;
			}
			break;
		}

		RecvBuffer[BytesRead] = 0;
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(RecvBuffer), BytesRead);
		MessageBuffer += FString(Converter.Length(), Converter.Get());

		FString Line;
		while (MessageBuffer.Split(TEXT("\n"), &Line, &MessageBuffer))
		{
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty()) continue;
			ProcessMessage(Line);
		}
	}

	bFinished = true;
	UE_LOG(LogN1MCP, Log, TEXT("Client %d disconnected"), ClientId);
	return 0;
}

void FN1MCPClientRunnable::ProcessMessage(const FString& Message)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogN1MCP, Warning, TEXT("Client %d: Failed to parse JSON"), ClientId);
		return;
	}

	FString Type;
	JsonObj->TryGetStringField(TEXT("type"), Type);

	if (Type == TEXT("hello"))
	{
		FString Response = HandleHandshake(JsonObj);
		SendString(Response);
		return;
	}

	if (!bHandshakeCompleted)
	{
		UE_LOG(LogN1MCP, Warning, TEXT("Client %d: Message before handshake"), ClientId);
		return;
	}

	if (Type != TEXT("request")) return;

	FString Id;
	JsonObj->TryGetStringField(TEXT("id"), Id);
	FString Command;
	JsonObj->TryGetStringField(TEXT("command"), Command);

	TSharedPtr<FJsonObject> Params;
	if (JsonObj->HasField(TEXT("params")))
	{
		Params = JsonObj->GetObjectField(TEXT("params"));
	}
	if (!Params.IsValid())
	{
		Params = MakeShared<FJsonObject>();
	}

	int32 TimeoutMs = 10000;
	if (JsonObj->HasField(TEXT("timeout_ms")))
	{
		TimeoutMs = static_cast<int32>(JsonObj->GetNumberField(TEXT("timeout_ms")));
	}

	// GameThread에서 커맨드 실행
	TSharedPtr<TPromise<TSharedPtr<FJsonObject>>> Promise =
		MakeShared<TPromise<TSharedPtr<FJsonObject>>>();
	TFuture<TSharedPtr<FJsonObject>> Future = Promise->GetFuture();

	AsyncTask(ENamedThreads::GameThread, [this, Command, Params, Promise]()
	{
		TSharedPtr<FJsonObject> CmdResult = OnCommandReceived.Execute(Command, Params);
		Promise->SetValue(CmdResult);
	});

	// WaitFor로 타임아웃 처리
	TSharedPtr<FJsonObject> Result;
	if (Future.WaitFor(FTimespan::FromMilliseconds(TimeoutMs)))
	{
		Result = Future.Get();
	}
	else
	{
		Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("status"), TEXT("error"));
		Result->SetStringField(TEXT("error_code"), TEXT("TIMEOUT"));
		Result->SetStringField(TEXT("error"),
			FString::Printf(TEXT("Command '%s' timed out after %d ms"), *Command, TimeoutMs));
	}

	Result->SetStringField(TEXT("type"), TEXT("response"));
	Result->SetStringField(TEXT("id"), Id);

	FString ResponseStr;
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResponseStr);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	Writer->Close();
	ResponseStr += TEXT("\n");

	SendString(ResponseStr);
}

FString FN1MCPClientRunnable::HandleHandshake(const TSharedPtr<FJsonObject>& HelloMsg)
{
	FString ClientVersion;
	HelloMsg->TryGetStringField(TEXT("protocol_version"), ClientVersion);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	if (ClientVersion != ProtocolVersion)
	{
		Response->SetStringField(TEXT("type"), TEXT("error"));
		Response->SetStringField(TEXT("error_code"), TEXT("INVALID_PARAMS"));
		Response->SetStringField(TEXT("error"),
			FString::Printf(TEXT("Protocol version mismatch: server=%s, client=%s"),
				ProtocolVersion, *ClientVersion));
		bHandshakeCompleted = false;
	}
	else
	{
		Response->SetStringField(TEXT("type"), TEXT("hello_ack"));
		Response->SetStringField(TEXT("protocol_version"), ProtocolVersion);
		Response->SetStringField(TEXT("server"), TEXT("N1UnrealMCP"));
		Response->SetStringField(TEXT("server_version"), ServerVersion);

		TArray<TSharedPtr<FJsonValue>> Capabilities;
		Capabilities.Add(MakeShared<FJsonValueString>(TEXT("pagination")));
		Capabilities.Add(MakeShared<FJsonValueString>(TEXT("meta_commands")));
		Response->SetArrayField(TEXT("capabilities"), Capabilities);

		bHandshakeCompleted = true;
		UE_LOG(LogN1MCP, Log, TEXT("Client %d: Handshake completed"), ClientId);
	}

	FString ResponseStr;
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResponseStr);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	Writer->Close();
	ResponseStr += TEXT("\n");
	return ResponseStr;
}

void FN1MCPClientRunnable::SendString(const FString& Str)
{
	if (!ClientSocket) return;
	FTCHARToUTF8 Converter(*Str);
	int32 BytesSent = 0;
	ClientSocket->Send(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length(), BytesSent);
}

// ═══════════════════════════════════════════════════════════════
// FN1MCPServerRunnable — 리스너 + 클라이언트 스레드 관리
// ═══════════════════════════════════════════════════════════════

FN1MCPServerRunnable::FN1MCPServerRunnable(uint16 InPort, FOnMCPCommandReceived InOnCommand)
	: Port(InPort)
	, OnCommandReceived(MoveTemp(InOnCommand))
	, bShouldRun(true)
{
}

FN1MCPServerRunnable::~FN1MCPServerRunnable()
{
	Stop();
}

void FN1MCPServerRunnable::Stop()
{
	bShouldRun = false;
	StopAllClients();
}

uint32 FN1MCPServerRunnable::Run()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogN1MCP, Error, TEXT("Failed to get socket subsystem"));
		return 1;
	}

	ListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("N1MCPListener"), false);
	if (!ListenerSocket)
	{
		UE_LOG(LogN1MCP, Error, TEXT("Failed to create listener socket"));
		return 1;
	}

	ListenerSocket->SetReuseAddr(true);
	ListenerSocket->SetNonBlocking(true);

	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	Addr->SetIp(FIPv4Address::InternalLoopback.Value);
	Addr->SetPort(Port);

	if (!ListenerSocket->Bind(*Addr))
	{
		UE_LOG(LogN1MCP, Error, TEXT("Failed to bind to port %d"), Port);
		SocketSubsystem->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
		return 1;
	}

	if (!ListenerSocket->Listen(5))
	{
		UE_LOG(LogN1MCP, Error, TEXT("Failed to listen on port %d"), Port);
		SocketSubsystem->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
		return 1;
	}

	UE_LOG(LogN1MCP, Log, TEXT("Listening on 127.0.0.1:%d (multi-client)"), Port);

	while (bShouldRun)
	{
		// 종료된 클라이언트 정리
		CleanupFinishedClients();

		bool bHasPending = false;
		if (ListenerSocket->HasPendingConnection(bHasPending) && bHasPending)
		{
			TSharedRef<FInternetAddr> ClientAddr = SocketSubsystem->CreateInternetAddr();
			FSocket* NewClientSocket = ListenerSocket->Accept(*ClientAddr, TEXT("N1MCPClient"));
			if (NewClientSocket)
			{
				int32 ClientId = NextClientId++;
				UE_LOG(LogN1MCP, Log, TEXT("Client %d connected (total: %d)"),
					ClientId, ClientSessions.Num() + 1);

				FClientSession Session;
				Session.Runnable = new FN1MCPClientRunnable(NewClientSocket, OnCommandReceived, ClientId);
				Session.Thread = FRunnableThread::Create(
					Session.Runnable,
					*FString::Printf(TEXT("N1MCPClient_%d"), ClientId));

				ClientSessions.Add(Session);
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.1f);
		}
	}

	StopAllClients();
	SocketSubsystem->DestroySocket(ListenerSocket);
	ListenerSocket = nullptr;
	return 0;
}

void FN1MCPServerRunnable::CleanupFinishedClients()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	for (int32 i = ClientSessions.Num() - 1; i >= 0; --i)
	{
		if (ClientSessions[i].Runnable && ClientSessions[i].Runnable->IsFinished())
		{
			if (ClientSessions[i].Thread)
			{
				ClientSessions[i].Thread->WaitForCompletion();
				delete ClientSessions[i].Thread;
			}
			delete ClientSessions[i].Runnable;
			ClientSessions.RemoveAt(i);
		}
	}
}

void FN1MCPServerRunnable::StopAllClients()
{
	for (FClientSession& Session : ClientSessions)
	{
		if (Session.Runnable)
		{
			Session.Runnable->Stop();
		}
		if (Session.Thread)
		{
			Session.Thread->WaitForCompletion();
			delete Session.Thread;
			Session.Thread = nullptr;
		}
		delete Session.Runnable;
		Session.Runnable = nullptr;
	}
	ClientSessions.Empty();
}
