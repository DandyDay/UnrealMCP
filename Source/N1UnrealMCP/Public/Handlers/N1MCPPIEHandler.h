#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class FN1MCPPIEHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPPIEHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	TSharedPtr<FJsonObject> HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleStopPIE(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleIsPIERunning(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params);

	bool IsCommandBlocked(const FString& Command) const;
	static TArray<FString> BlockedPrefixes;
};
