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

bool FN1MCPServerRunnable::Init()
{
	return true;
}

void FN1MCPServerRunnable::Stop()
{
	bShouldRun = false;
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

	if (!ListenerSocket->Listen(1))
	{
		UE_LOG(LogN1MCP, Error, TEXT("Failed to listen on port %d"), Port);
		SocketSubsystem->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
		return 1;
	}

	UE_LOG(LogN1MCP, Log, TEXT("Listening on 127.0.0.1:%d"), Port);

	while (bShouldRun)
	{
		bool bHasPending = false;
		if (ListenerSocket->HasPendingConnection(bHasPending) && bHasPending)
		{
			TSharedRef<FInternetAddr> ClientAddr = SocketSubsystem->CreateInternetAddr();
			FSocket* ClientSocket = ListenerSocket->Accept(*ClientAddr, TEXT("N1MCPClient"));
			if (ClientSocket)
			{
				UE_LOG(LogN1MCP, Log, TEXT("Client connected"));
				bHandshakeCompleted = false;
				HandleClient(ClientSocket);
				UE_LOG(LogN1MCP, Log, TEXT("Client disconnected"));
				SocketSubsystem->DestroySocket(ClientSocket);
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.1f);
		}
	}

	SocketSubsystem->DestroySocket(ListenerSocket);
	ListenerSocket = nullptr;
	return 0;
}

void FN1MCPServerRunnable::HandleClient(FSocket* Client)
{
	Client->SetNonBlocking(false);

	FString MessageBuffer;
	uint8 RecvBuffer[4096];

	while (bShouldRun)
	{
		int32 BytesRead = 0;
		if (!Client->Recv(RecvBuffer, sizeof(RecvBuffer) - 1, BytesRead))
		{
			// 연결 끊김 또는 에러
			break;
		}

		if (BytesRead <= 0)
		{
			// would block 또는 연결 종료
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

		// \n으로 메시지 분리
		FString Line;
		while (MessageBuffer.Split(TEXT("\n"), &Line, &MessageBuffer))
		{
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty()) continue;
			ProcessMessage(Line, Client);
		}
	}
}

void FN1MCPServerRunnable::ProcessMessage(const FString& Message, FSocket* Client)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogN1MCP, Warning, TEXT("Failed to parse JSON message"));
		return;
	}

	FString Type;
	JsonObj->TryGetStringField(TEXT("type"), Type);

	// 핸드셰이크
	if (Type == TEXT("hello"))
	{
		FString Response = HandleHandshake(JsonObj);
		SendString(Client, Response);
		return;
	}

	// 핸드셰이크 전에는 request 거부
	if (!bHandshakeCompleted)
	{
		UE_LOG(LogN1MCP, Warning, TEXT("Received message before handshake"));
		return;
	}

	if (Type != TEXT("request")) return;

	FString Id;
	JsonObj->TryGetStringField(TEXT("id"), Id);
	FString Command;
	JsonObj->TryGetStringField(TEXT("command"), Command);

	// params가 없으면 빈 오브젝트
	TSharedPtr<FJsonObject> Params;
	if (JsonObj->HasField(TEXT("params")))
	{
		Params = JsonObj->GetObjectField(TEXT("params"));
	}
	if (!Params.IsValid())
	{
		Params = MakeShared<FJsonObject>();
	}

	// 타임아웃: 요청에서 읽거나 기본값 사용
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

	// type + id 에코
	Result->SetStringField(TEXT("type"), TEXT("response"));
	Result->SetStringField(TEXT("id"), Id);

	FString ResponseStr;
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResponseStr);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	Writer->Close();
	ResponseStr += TEXT("\n");

	SendString(Client, ResponseStr);
}

FString FN1MCPServerRunnable::HandleHandshake(const TSharedPtr<FJsonObject>& HelloMsg)
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
		UE_LOG(LogN1MCP, Log, TEXT("Handshake completed (protocol %s)"), ProtocolVersion);
	}

	FString ResponseStr;
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResponseStr);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
	Writer->Close();
	ResponseStr += TEXT("\n");
	return ResponseStr;
}

void FN1MCPServerRunnable::SendString(FSocket* Socket, const FString& Str)
{
	FTCHARToUTF8 Converter(*Str);
	int32 BytesSent = 0;
	Socket->Send(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length(), BytesSent);
}
