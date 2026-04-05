#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class FN1MCPMaterialHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPMaterialHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnectMaterialNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialExpressionParam(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectToMaterialOutput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialInstanceParam(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params);

	UMaterial* FindMaterial(const FString& Path, FString& OutError);
	UMaterialExpression* FindExpressionByIndex(UMaterial* Mat, int32 Index);
	TSharedPtr<FJsonObject> ExpressionToJson(UMaterialExpression* Expr, int32 Index);
};
