#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class FN1MCPBlueprintHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPBlueprintHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	TSharedPtr<FJsonObject> HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleOpenBlueprint(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddComponent(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveComponent(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetComponents(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetBlueprintInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddVariable(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveVariable(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetVariableDefault(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddInterface(const TSharedPtr<FJsonObject>& Params);

	// BP 검색 헬퍼 (풀 경로 또는 짧은 이름)
	UBlueprint* FindBlueprintByPath(const FString& Path, FString& OutError);
};
