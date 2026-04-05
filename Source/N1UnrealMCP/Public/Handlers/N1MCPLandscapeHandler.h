#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class FN1MCPLandscapeHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPLandscapeHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	TSharedPtr<FJsonObject> HandleCreateLandscape(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddLandscapeLayer(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSculptLandscape(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePaintLandscapeLayer(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleImportHeightmap(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleExportHeightmap(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAssignLandscapeMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params);

	class ALandscapeProxy* FindLandscape(const FString& Ref, FString& OutError);
};
