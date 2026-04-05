#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class FN1MCPUMGHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPUMGHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	TSharedPtr<FJsonObject> HandleCreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddWidget(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveWidget(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetWidgetLayout(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetWidgetTree(const TSharedPtr<FJsonObject>& Params);

	class UWidgetBlueprint* FindWidgetBP(const FString& Path, FString& OutError);
	TSharedPtr<FJsonObject> WidgetToJson(class UWidget* Widget);
};
