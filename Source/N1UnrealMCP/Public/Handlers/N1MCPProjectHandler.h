#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class FN1MCPProjectHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPProjectHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetPluginsList(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetPluginEnabled(const TSharedPtr<FJsonObject>& Params);
};
