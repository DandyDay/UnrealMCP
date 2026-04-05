#include "Handlers/N1MCPBlueprintHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditorLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/SavePackage.h"
#include "Subsystems/AssetEditorSubsystem.h"

FN1MCPBlueprintHandler::FN1MCPBlueprintHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("blueprint"))
{
}

UBlueprint* FN1MCPBlueprintHandler::FindBlueprintByPath(const FString& Path, FString& OutError)
{
	FString AssetPath = Path;
	if (!AssetPath.StartsWith(TEXT("/")))
	{
		AssetPath = TEXT("/Game/Blueprints/") + AssetPath;
	}

	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!BP)
	{
		OutError = FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath);
	}
	return BP;
}

void FN1MCPBlueprintHandler::RegisterCommands()
{
	RegisterCommand(TEXT("create_blueprint"),
		TEXT("새 블루프린트 에셋 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateBlueprint(P); });

	RegisterCommand(TEXT("open_blueprint"),
		TEXT("에디터에서 블루프린트 열기"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleOpenBlueprint(P); });

	RegisterCommand(TEXT("compile_blueprint"),
		TEXT("블루프린트 컴파일"), nullptr,
		false, false, true, 120000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCompileBlueprint(P); });

	RegisterCommand(TEXT("add_component"),
		TEXT("블루프린트에 컴포넌트 추가"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddComponent(P); });

	RegisterCommand(TEXT("remove_component"),
		TEXT("블루프린트에서 컴포넌트 제거"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleRemoveComponent(P); });

	RegisterCommand(TEXT("get_components"),
		TEXT("블루프린트 컴포넌트 목록 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetComponents(P); });

	RegisterCommand(TEXT("set_component_property"),
		TEXT("블루프린트 컴포넌트 속성 수정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetComponentProperty(P); });

	RegisterCommand(TEXT("get_blueprint_info"),
		TEXT("블루프린트 전체 정보 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetBlueprintInfo(P); });

	RegisterCommand(TEXT("add_variable"),
		TEXT("블루프린트에 변수 추가"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddVariable(P); });

	RegisterCommand(TEXT("remove_variable"),
		TEXT("블루프린트에서 변수 제거"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleRemoveVariable(P); });

	RegisterCommand(TEXT("set_variable_default"),
		TEXT("블루프린트 변수 기본값 설정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetVariableDefault(P); });

	RegisterCommand(TEXT("add_interface"),
		TEXT("블루프린트에 인터페이스 구현 추가"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddInterface(P); });
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString Name, ParentClassStr;
	if (!Params || !Params->TryGetStringField(TEXT("name"), Name))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'name' required"));
	if (!Params->TryGetStringField(TEXT("parent_class"), ParentClassStr))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'parent_class' required"));

	FString Path = TEXT("/Game/Blueprints");
	Params->TryGetStringField(TEXT("path"), Path);

	// 부모 클래스 해석
	UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassStr);
	if (!ParentClass)
		ParentClass = LoadClass<AActor>(nullptr, *ParentClassStr);
	if (!ParentClass)
	{
		FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ParentClassStr);
		ParentClass = LoadClass<AActor>(nullptr, *FullPath);
	}
	if (!ParentClass)
	{
		// A 접두사 시도
		FString WithPrefix = FString::Printf(TEXT("/Script/Engine.A%s"), *ParentClassStr);
		ParentClass = LoadClass<AActor>(nullptr, *WithPrefix);
	}
	if (!ParentClass)
		ParentClass = AActor::StaticClass();

	FString PackagePath = Path / Name;

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create package"));

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UObject* Asset = Factory->FactoryCreateNew(
		UBlueprint::StaticClass(), Package, FName(*Name),
		RF_Public | RF_Standalone, nullptr, GWarn);

	UBlueprint* BP = Cast<UBlueprint>(Asset);
	if (!BP)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create blueprint"));

	FAssetRegistryModule::AssetCreated(BP);
	BP->MarkPackageDirty();

	FKismetEditorUtilities::CompileBlueprint(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), BP->GetPathName());
	Data->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleOpenBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("blueprint"), BP->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	FKismetEditorUtilities::CompileBlueprint(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), BP->GetPathName());
	Data->SetBoolField(TEXT("compiled"), true);

	bool bHasErrors = BP->Status == BS_Error;
	Data->SetBoolField(TEXT("has_errors"), bHasErrors);

	if (bHasErrors)
		return ErrorResponse(TEXT("BLUEPRINT_COMPILE_ERROR"),
			FString::Printf(TEXT("Blueprint '%s' has compilation errors"), *BPPath));

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleAddComponent(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, CompClassStr;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("component_class"), CompClassStr))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'component_class' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UClass* CompClass = FindObject<UClass>(nullptr, *CompClassStr);
	if (!CompClass)
		CompClass = LoadClass<UActorComponent>(nullptr, *CompClassStr);
	if (!CompClass)
	{
		FString FullPath = FString::Printf(TEXT("/Script/Engine.U%s"), *CompClassStr);
		CompClass = LoadClass<UActorComponent>(nullptr, *FullPath);
	}
	if (!CompClass)
	{
		FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *CompClassStr);
		CompClass = LoadClass<UActorComponent>(nullptr, *FullPath);
	}
	if (!CompClass)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Component class '%s' not found"), *CompClassStr));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_component")));

	USCS_Node* NewNode = BP->SimpleConstructionScript->CreateNode(CompClass, NAME_None);
	if (!NewNode)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create SCS node"));

	FString CompName;
	if (Params->TryGetStringField(TEXT("name"), CompName))
	{
		NewNode->SetVariableName(FName(*CompName));
	}

	BP->SimpleConstructionScript->AddNode(NewNode);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("component_name"), NewNode->GetVariableName().ToString());
	Data->SetStringField(TEXT("component_class"), CompClass->GetName());
	Data->SetStringField(TEXT("blueprint"), BP->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleRemoveComponent(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, CompName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("component_name"), CompName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'component_name' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	USCS_Node* FoundNode = nullptr;
	for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
	{
		if (Node->GetVariableName().ToString() == CompName)
		{
			FoundNode = Node;
			break;
		}
	}

	if (!FoundNode)
		return ErrorResponse(TEXT("COMPONENT_NOT_FOUND"),
			FString::Printf(TEXT("Component '%s' not found"), *CompName));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: remove_component")));
	BP->SimpleConstructionScript->RemoveNode(FoundNode);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("removed"), CompName);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleGetComponents(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	TArray<TSharedPtr<FJsonValue>> CompArray;
	for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
	{
		TSharedPtr<FJsonObject> Comp = MakeShared<FJsonObject>();
		Comp->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
		Comp->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("None"));
		CompArray.Add(MakeShared<FJsonValueObject>(Comp));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("components"), CompArray);
	Data->SetNumberField(TEXT("count"), CompArray.Num());
	Data->SetStringField(TEXT("blueprint"), BP->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, CompName, PropName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("component_name"), CompName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'component_name' required"));
	if (!Params->TryGetStringField(TEXT("property"), PropName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'property' required"));
	if (!Params->HasField(TEXT("value")))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'value' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	USCS_Node* FoundNode = nullptr;
	for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
	{
		if (Node->GetVariableName().ToString() == CompName)
		{
			FoundNode = Node;
			break;
		}
	}
	if (!FoundNode)
		return ErrorResponse(TEXT("COMPONENT_NOT_FOUND"),
			FString::Printf(TEXT("Component '%s' not found"), *CompName));

	UActorComponent* Template = FoundNode->ComponentTemplate;
	if (!Template)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Component template is null"));

	FProperty* Prop = Template->GetClass()->FindPropertyByName(FName(*PropName));
	if (!Prop)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Property '%s' not found on component"), *PropName));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_component_property")));
	Template->Modify();

	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Template);

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		BoolProp->SetPropertyValue(PropAddr, Value->AsBool());
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		FloatProp->SetPropertyValue(PropAddr, static_cast<float>(Value->AsNumber()));
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		DoubleProp->SetPropertyValue(PropAddr, Value->AsNumber());
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		IntProp->SetPropertyValue(PropAddr, static_cast<int32>(Value->AsNumber()));
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		StrProp->SetPropertyValue(PropAddr, Value->AsString());
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
	{
		FString AssetPath = Value->AsString();
		UObject* LoadedObj = StaticLoadObject(ObjProp->PropertyClass, nullptr, *AssetPath);
		if (!LoadedObj)
			return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
				FString::Printf(TEXT("Asset '%s' not found"), *AssetPath));
		ObjProp->SetObjectPropertyValue(PropAddr, LoadedObj);
	}
	else
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Unsupported property type for '%s'"), *PropName));

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("component"), CompName);
	Data->SetStringField(TEXT("property"), PropName);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleGetBlueprintInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), BP->GetPathName());
	Data->SetStringField(TEXT("parent_class"), BP->ParentClass ? BP->ParentClass->GetName() : TEXT("None"));

	// 변수 목록
	TArray<TSharedPtr<FJsonValue>> VarArray;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		VarObj->SetBoolField(TEXT("editable"), Var.PropertyFlags & CPF_Edit ? true : false);
		VarArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}
	Data->SetArrayField(TEXT("variables"), VarArray);

	// 컴포넌트 목록
	TArray<TSharedPtr<FJsonValue>> CompArray;
	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			TSharedPtr<FJsonObject> Comp = MakeShared<FJsonObject>();
			Comp->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			Comp->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("None"));
			CompArray.Add(MakeShared<FJsonValueObject>(Comp));
		}
	}
	Data->SetArrayField(TEXT("components"), CompArray);

	// 인터페이스 목록
	TArray<TSharedPtr<FJsonValue>> IntfArray;
	for (const FBPInterfaceDescription& Intf : BP->ImplementedInterfaces)
	{
		if (Intf.Interface)
			IntfArray.Add(MakeShared<FJsonValueString>(Intf.Interface->GetName()));
	}
	Data->SetArrayField(TEXT("interfaces"), IntfArray);

	Data->SetBoolField(TEXT("has_errors"), BP->Status == BS_Error);

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleAddVariable(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, VarName, VarType;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("var_name"), VarName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'var_name' required"));
	if (!Params->TryGetStringField(TEXT("var_type"), VarType))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'var_type' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	// 타입 매핑
	FEdGraphPinType PinType;
	if (VarType == TEXT("bool") || VarType == TEXT("Boolean"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (VarType == TEXT("int") || VarType == TEXT("Integer"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (VarType == TEXT("float") || VarType == TEXT("Float"))
	{
		// UE 5.7: PC_Real은 subcategory 필수
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (VarType == TEXT("double") || VarType == TEXT("Double"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (VarType == TEXT("string") || VarType == TEXT("String"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (VarType == TEXT("vector") || VarType == TEXT("Vector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (VarType == TEXT("rotator") || VarType == TEXT("Rotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_variable")));

	FName VarFName(*VarName);
	bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(BP, VarFName, PinType);

	if (!bSuccess)
		return ErrorResponse(TEXT("INTERNAL_ERROR"),
			FString::Printf(TEXT("Failed to add variable '%s'"), *VarName));

	// editable 설정
	bool bEditable = false;
	if (Params->HasField(TEXT("editable")))
	{
		bEditable = Params->GetBoolField(TEXT("editable"));
		if (bEditable)
		{
			FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, VarFName, false);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("variable"), VarName);
	Data->SetStringField(TEXT("type"), VarType);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleRemoveVariable(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, VarName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("var_name"), VarName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'var_name' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: remove_variable")));
	FBlueprintEditorUtils::RemoveMemberVariable(BP, FName(*VarName));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("removed"), VarName);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleSetVariableDefault(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, VarName;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("var_name"), VarName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'var_name' required"));
	if (!Params->HasField(TEXT("value")))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'value' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	FString DefaultValue;
	if (Value->Type == EJson::String)
		DefaultValue = Value->AsString();
	else if (Value->Type == EJson::Number)
		DefaultValue = FString::SanitizeFloat(Value->AsNumber());
	else if (Value->Type == EJson::Boolean)
		DefaultValue = Value->AsBool() ? TEXT("true") : TEXT("false");

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_variable_default")));

	bool bFound = false;
	for (FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarName == FName(*VarName))
		{
			Var.DefaultValue = DefaultValue;
			bFound = true;
			break;
		}
	}

	if (!bFound)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Variable '%s' not found"), *VarName));

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("variable"), VarName);
	Data->SetStringField(TEXT("default_value"), DefaultValue);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPBlueprintHandler::HandleAddInterface(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath, IntfPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));
	if (!Params->TryGetStringField(TEXT("interface_path"), IntfPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'interface_path' required"));

	FString Error;
	UBlueprint* BP = FindBlueprintByPath(BPPath, Error);
	if (!BP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UClass* IntfClass = LoadClass<UInterface>(nullptr, *IntfPath);
	if (!IntfClass)
	{
		FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *IntfPath);
		IntfClass = LoadClass<UInterface>(nullptr, *FullPath);
	}
	if (!IntfClass)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Interface '%s' not found"), *IntfPath));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_interface")));
	FBlueprintEditorUtils::ImplementNewInterface(BP, IntfClass->GetClassPathName());
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("interface"), IntfClass->GetName());
	Data->SetStringField(TEXT("blueprint"), BP->GetPathName());
	return SuccessResponse(Data);
}
