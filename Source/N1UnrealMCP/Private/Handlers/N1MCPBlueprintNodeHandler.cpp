#include "Handlers/N1MCPBlueprintNodeHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputAction.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"

FN1MCPBlueprintNodeHandler::FN1MCPBlueprintNodeHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("blueprint_node"))
{
}

// ── 헬퍼 ──

UBlueprint* FN1MCPBlueprintNodeHandler::FindBP(const FString& Path, FString& OutError)
{
	FString AssetPath = Path;
	if (!AssetPath.StartsWith(TEXT("/")))
		AssetPath = TEXT("/Game/Blueprints/") + AssetPath;

	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!BP)
		OutError = FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath);
	return BP;
}

UEdGraph* FN1MCPBlueprintNodeHandler::FindEventGraph(UBlueprint* BP)
{
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		if (Graph->GetName().Contains(TEXT("EventGraph")))
			return Graph;
	}
	if (BP->UbergraphPages.Num() > 0)
		return BP->UbergraphPages[0];
	return nullptr;
}

UK2Node* FN1MCPBlueprintNodeHandler::FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr)
{
	FGuid Guid;
	if (!FGuid::Parse(GuidStr, Guid))
		return nullptr;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node->NodeGuid == Guid)
			return Cast<UK2Node>(Node);
	}
	return nullptr;
}

FVector2D FN1MCPBlueprintNodeHandler::GetPosition(const TSharedPtr<FJsonObject>& Params)
{
	FVector2D Pos(0, 0);
	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (Params.IsValid() && Params->TryGetArrayField(TEXT("position"), Arr) && Arr->Num() >= 2)
	{
		Pos.X = (*Arr)[0]->AsNumber();
		Pos.Y = (*Arr)[1]->AsNumber();
	}
	return Pos;
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::PinToJson(UEdGraphPin* Pin)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Pin->PinName.ToString());
	Obj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
	Obj->SetStringField(TEXT("direction"),
		Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
	Obj->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
	return Obj;
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::NodeToJson(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
	Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	Obj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Obj->SetNumberField(TEXT("x"), Node->NodePosX);
	Obj->SetNumberField(TEXT("y"), Node->NodePosY);

	TArray<TSharedPtr<FJsonValue>> PinsArr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin->bHidden)
			PinsArr.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
	}
	Obj->SetArrayField(TEXT("pins"), PinsArr);
	return Obj;
}

// ── 등록 ──

void FN1MCPBlueprintNodeHandler::RegisterCommands()
{
	RegisterCommand(TEXT("add_event_node"),
		TEXT("이벤트 노드 생성 (BeginPlay, Tick 등)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddEventNode(P); });

	RegisterCommand(TEXT("add_function_call_node"),
		TEXT("함수 호출 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddFunctionCallNode(P); });

	RegisterCommand(TEXT("add_variable_get_node"),
		TEXT("변수 Get 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddVariableGetNode(P); });

	RegisterCommand(TEXT("add_variable_set_node"),
		TEXT("변수 Set 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddVariableSetNode(P); });

	RegisterCommand(TEXT("add_self_reference_node"),
		TEXT("Self 참조 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddSelfReferenceNode(P); });

	RegisterCommand(TEXT("add_component_reference_node"),
		TEXT("컴포넌트 참조 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddComponentReferenceNode(P); });

	RegisterCommand(TEXT("add_input_action_node"),
		TEXT("Enhanced Input 액션 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddInputActionNode(P); });

	RegisterCommand(TEXT("add_pure_math_node"),
		TEXT("수학 연산 노드 생성 (Add, Multiply 등)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddPureMathNode(P); });

	RegisterCommand(TEXT("add_branch_node"),
		TEXT("Branch (if) 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddBranchNode(P); });

	RegisterCommand(TEXT("add_macro_node"),
		TEXT("매크로 노드 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddMacroNode(P); });

	RegisterCommand(TEXT("connect_nodes"),
		TEXT("노드 핀 연결"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleConnectNodes(P); });

	RegisterCommand(TEXT("disconnect_nodes"),
		TEXT("노드 핀 연결 해제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDisconnectNodes(P); });

	RegisterCommand(TEXT("find_nodes"),
		TEXT("그래프 내 노드 검색"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleFindNodes(P); });

	RegisterCommand(TEXT("delete_node"),
		TEXT("노드 삭제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDeleteNode(P); });

	RegisterCommand(TEXT("get_node_pins"),
		TEXT("노드의 핀 정보 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetNodePins(P); });
}

// ── 커맨드 구현 ──

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddEventNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, EventName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'event_name' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FVector2D Pos = GetPosition(Params);

	// 기존 이벤트 노드 검색
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
		if (EventNode && EventNode->GetFunctionName() == FName(*EventName))
			return SuccessResponse(NodeToJson(EventNode));
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_event_node")));

	UK2Node_Event* NewNode = NewObject<UK2Node_Event>(Graph);
	NewNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
	NewNode->bOverrideFunction = true;
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddFunctionCallNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, FuncName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("function_name"), FuncName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'function_name' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	// 대상 클래스 결정
	UClass* TargetClass = BP->GeneratedClass ? static_cast<UClass*>(BP->GeneratedClass) : AActor::StaticClass();
	FString TargetClassStr;
	if (Params->TryGetStringField(TEXT("target_class"), TargetClassStr))
	{
		UClass* CustomClass = FindObject<UClass>(nullptr, *TargetClassStr);
		if (!CustomClass)
		{
			FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *TargetClassStr);
			CustomClass = LoadClass<UObject>(nullptr, *FullPath);
		}
		if (CustomClass) TargetClass = CustomClass;
	}

	UFunction* Func = TargetClass->FindFunctionByName(FName(*FuncName));
	if (!Func)
	{
		// 엔진 라이브러리에서 검색
		Func = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName(*FuncName));
		if (!Func)
			Func = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(*FuncName));
		if (!Func)
			Func = UGameplayStatics::StaticClass()->FindFunctionByName(FName(*FuncName));
	}

	if (!Func)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Function '%s' not found"), *FuncName));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_function_call_node")));

	UK2Node_CallFunction* NewNode = NewObject<UK2Node_CallFunction>(Graph);
	NewNode->FunctionReference.SetExternalMember(Func->GetFName(), Func->GetOwnerClass());
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddVariableGetNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, VarName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("var_name"), VarName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'var_name' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_variable_get_node")));

	UK2Node_VariableGet* NewNode = NewObject<UK2Node_VariableGet>(Graph);
	NewNode->VariableReference.SetSelfMember(FName(*VarName));
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddVariableSetNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, VarName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("var_name"), VarName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'var_name' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_variable_set_node")));

	UK2Node_VariableSet* NewNode = NewObject<UK2Node_VariableSet>(Graph);
	NewNode->VariableReference.SetSelfMember(FName(*VarName));
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddSelfReferenceNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_self_reference_node")));

	UK2Node_Self* NewNode = NewObject<UK2Node_Self>(Graph);
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddComponentReferenceNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, CompName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("component_name"), CompName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'component_name' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_component_reference_node")));

	UK2Node_VariableGet* NewNode = NewObject<UK2Node_VariableGet>(Graph);
	NewNode->VariableReference.SetSelfMember(FName(*CompName));
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddInputActionNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, ActionPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("input_action_path"), ActionPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'input_action_path' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_input_action_node")));

	UK2Node_InputAction* NewNode = NewObject<UK2Node_InputAction>(Graph);
	NewNode->InputActionName = FName(*ActionPath);
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddPureMathNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, Operation;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("operation"), Operation))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'operation' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	// 수학 연산을 KismetMathLibrary에서 검색
	FString FuncName = Operation;
	// 일반적인 매핑
	if (Operation == TEXT("Add")) FuncName = TEXT("Add_FloatFloat");
	else if (Operation == TEXT("Subtract")) FuncName = TEXT("Subtract_FloatFloat");
	else if (Operation == TEXT("Multiply")) FuncName = TEXT("Multiply_FloatFloat");
	else if (Operation == TEXT("Divide")) FuncName = TEXT("Divide_FloatFloat");

	UFunction* Func = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName(*FuncName));
	if (!Func)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Math operation '%s' not found"), *Operation));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_pure_math_node")));

	UK2Node_CallFunction* NewNode = NewObject<UK2Node_CallFunction>(Graph);
	NewNode->FunctionReference.SetExternalMember(Func->GetFName(), Func->GetOwnerClass());
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddBranchNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_branch_node")));

	UK2Node_IfThenElse* NewNode = NewObject<UK2Node_IfThenElse>(Graph);
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleAddMacroNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, MacroName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("macro_name"), MacroName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'macro_name' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	// 매크로 그래프 검색
	UEdGraph* MacroGraph = nullptr;
	for (UEdGraph* MG : BP->MacroGraphs)
	{
		if (MG->GetName() == MacroName)
		{
			MacroGraph = MG;
			break;
		}
	}
	if (!MacroGraph)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Macro '%s' not found in blueprint"), *MacroName));

	FVector2D Pos = GetPosition(Params);
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_macro_node")));

	UK2Node_MacroInstance* NewNode = NewObject<UK2Node_MacroInstance>(Graph);
	NewNode->SetMacroGraph(MacroGraph);
	NewNode->CreateNewGuid();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = Pos.X;
	NewNode->NodePosY = Pos.Y;
	Graph->AddNode(NewNode, false, false);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	return SuccessResponse(NodeToJson(NewNode));
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, SrcNodeId, SrcPinName, DstNodeId, DstPinName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("source_node_id"), SrcNodeId))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'source_node_id' required"));
	if (!Params->TryGetStringField(TEXT("source_pin"), SrcPinName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'source_pin' required"));
	if (!Params->TryGetStringField(TEXT("target_node_id"), DstNodeId))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'target_node_id' required"));
	if (!Params->TryGetStringField(TEXT("target_pin"), DstPinName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'target_pin' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	UK2Node* SrcNode = FindNodeByGuid(Graph, SrcNodeId);
	if (!SrcNode) return ErrorResponse(TEXT("NODE_NOT_FOUND"),
		FString::Printf(TEXT("Source node '%s' not found"), *SrcNodeId));

	UK2Node* DstNode = FindNodeByGuid(Graph, DstNodeId);
	if (!DstNode) return ErrorResponse(TEXT("NODE_NOT_FOUND"),
		FString::Printf(TEXT("Target node '%s' not found"), *DstNodeId));

	UEdGraphPin* SrcPin = SrcNode->FindPin(FName(*SrcPinName), EGPD_Output);
	if (!SrcPin) SrcPin = SrcNode->FindPin(FName(*SrcPinName));
	if (!SrcPin) return ErrorResponse(TEXT("PIN_NOT_FOUND"),
		FString::Printf(TEXT("Source pin '%s' not found"), *SrcPinName));

	UEdGraphPin* DstPin = DstNode->FindPin(FName(*DstPinName), EGPD_Input);
	if (!DstPin) DstPin = DstNode->FindPin(FName(*DstPinName));
	if (!DstPin) return ErrorResponse(TEXT("PIN_NOT_FOUND"),
		FString::Printf(TEXT("Target pin '%s' not found"), *DstPinName));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: connect_nodes")));

	bool bConnected = Graph->GetSchema()->TryCreateConnection(SrcPin, DstPin);
	if (!bConnected)
		return ErrorResponse(TEXT("CONNECTION_FAILED"), TEXT("Failed to connect pins"));

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("source_node"), SrcNodeId);
	Data->SetStringField(TEXT("source_pin"), SrcPinName);
	Data->SetStringField(TEXT("target_node"), DstNodeId);
	Data->SetStringField(TEXT("target_pin"), DstPinName);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleDisconnectNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, NodeId, PinName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'node_id' required"));
	if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'pin_name' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	UK2Node* Node = FindNodeByGuid(Graph, NodeId);
	if (!Node) return ErrorResponse(TEXT("NODE_NOT_FOUND"),
		FString::Printf(TEXT("Node '%s' not found"), *NodeId));

	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin) return ErrorResponse(TEXT("PIN_NOT_FOUND"),
		FString::Printf(TEXT("Pin '%s' not found"), *PinName));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: disconnect_nodes")));
	Pin->BreakAllPinLinks();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleFindNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	FString Filter;
	Params->TryGetStringField(TEXT("filter"), Filter);

	TArray<TSharedPtr<FJsonValue>> AllNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Filter.IsEmpty())
		{
			FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			FString ClassName = Node->GetClass()->GetName();
			if (!Title.Contains(Filter) && !ClassName.Contains(Filter))
				continue;
		}
		AllNodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
	}

	TSharedPtr<FJsonObject> Data = FN1MCPCommandRegistry::ApplyPagination(AllNodes, Params);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleDeleteNode(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, NodeId;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'node_id' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	UK2Node* Node = FindNodeByGuid(Graph, NodeId);
	if (!Node) return ErrorResponse(TEXT("NODE_NOT_FOUND"),
		FString::Printf(TEXT("Node '%s' not found"), *NodeId));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: delete_node")));
	Node->DestroyNode();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("deleted_node"), NodeId);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintNodeHandler::HandleGetNodePins(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, NodeId;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'node_id' required"));

	FString Error;
	UBlueprint* BP = FindBP(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UEdGraph* Graph = FindEventGraph(BP);
	if (!Graph) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No event graph found"));

	UK2Node* Node = FindNodeByGuid(Graph, NodeId);
	if (!Node) return ErrorResponse(TEXT("NODE_NOT_FOUND"),
		FString::Printf(TEXT("Node '%s' not found"), *NodeId));

	TArray<TSharedPtr<FJsonValue>> PinsArr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		PinsArr.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("node_id"), NodeId);
	Data->SetArrayField(TEXT("pins"), PinsArr);
	Data->SetNumberField(TEXT("count"), PinsArr.Num());
	return SuccessResponse(Data);
}
