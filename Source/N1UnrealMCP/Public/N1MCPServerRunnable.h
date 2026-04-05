#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Dom/JsonObject.h"

class FSocket;

DECLARE_DELEGATE_RetVal_TwoParams(
	TSharedPtr<FJsonObject>,
	FOnMCPCommandReceived,
	const FString&,
	const TSharedPtr<FJsonObject>&
);

// 개별 클라이언트 세션을 처리하는 스레드
class FN1MCPClientRunnable : public FRunnable
{
public:
	FN1MCPClientRunnable(FSocket* InSocket, FOnMCPCommandReceived InOnCommand, int32 InClientId);
	virtual ~FN1MCPClientRunnable();

	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override;

	bool IsFinished() const { return bFinished; }
	int32 GetClientId() const { return ClientId; }

private:
	FSocket* ClientSocket;
	FOnMCPCommandReceived OnCommandReceived;
	FThreadSafeBool bShouldRun;
	FThreadSafeBool bFinished;
	int32 ClientId;
	bool bHandshakeCompleted = false;

	void ProcessMessage(const FString& Message);
	FString HandleHandshake(const TSharedPtr<FJsonObject>& HelloMsg);
	void SendString(const FString& Str);

	static constexpr const TCHAR* ProtocolVersion = TEXT("1.0");
	static constexpr const TCHAR* ServerVersion = TEXT("1.0.0");
};

// 리스너 소켓 — 접속을 받고 클라이언트 스레드를 생성
class FN1MCPServerRunnable : public FRunnable
{
public:
	FN1MCPServerRunnable(uint16 InPort, FOnMCPCommandReceived InOnCommand);
	virtual ~FN1MCPServerRunnable();

	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	uint16 Port;
	FOnMCPCommandReceived OnCommandReceived;
	FThreadSafeBool bShouldRun;
	FSocket* ListenerSocket = nullptr;

	// 활성 클라이언트 관리
	struct FClientSession
	{
		FN1MCPClientRunnable* Runnable = nullptr;
		FRunnableThread* Thread = nullptr;
	};
	TArray<FClientSession> ClientSessions;
	int32 NextClientId = 0;

	void CleanupFinishedClients();
	void StopAllClients();
};
