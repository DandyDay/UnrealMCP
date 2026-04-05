#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class UEdGraph;
class UBlueprint;
class UK2Node;

class FN1MCPBlueprintNodeHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPBlueprintNodeHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	TSharedPtr<FJsonObject> HandleAddEventNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddFunctionCallNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddVariableGetNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddVariableSetNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddSelfReferenceNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddComponentReferenceNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddInputActionNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddPureMathNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddBranchNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddMacroNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnectNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindNodes(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteNode(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetNodePins(const TSharedPtr<FJsonObject>& Params);

	// 헬퍼
	UBlueprint* FindBP(const FString& Path, FString& OutError);
	UEdGraph* FindEventGraph(UBlueprint* BP);
	UK2Node* FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr);
	TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node);
	TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin);
	FVector2D GetPosition(const TSharedPtr<FJsonObject>& Params);
};
