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

class FN1MCPServerRunnable : public FRunnable
{
public:
	FN1MCPServerRunnable(uint16 InPort, FOnMCPCommandReceived InOnCommand);
	virtual ~FN1MCPServerRunnable();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	uint16 Port;
	FOnMCPCommandReceived OnCommandReceived;
	FThreadSafeBool bShouldRun;
	FSocket* ListenerSocket = nullptr;

	void HandleClient(FSocket* Client);
	void ProcessMessage(const FString& Message, FSocket* Client);
	FString HandleHandshake(const TSharedPtr<FJsonObject>& HelloMsg);
	void SendString(FSocket* Socket, const FString& Str);

	bool bHandshakeCompleted = false;
	static constexpr const TCHAR* ProtocolVersion = TEXT("1.0");
	static constexpr const TCHAR* ServerVersion = TEXT("1.0.0");
};
