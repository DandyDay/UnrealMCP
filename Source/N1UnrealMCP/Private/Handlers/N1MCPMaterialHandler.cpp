#include "Handlers/N1MCPMaterialHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

FN1MCPMaterialHandler::FN1MCPMaterialHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("material"))
{
}

UMaterial* FN1MCPMaterialHandler::FindMaterial(const FString& Path, FString& OutError)
{
	FString AssetPath = Path;
	if (!AssetPath.StartsWith(TEXT("/")))
		AssetPath = TEXT("/Game/Materials/") + AssetPath;
	UMaterial* Mat = LoadObject<UMaterial>(nullptr, *AssetPath);
	if (!Mat) OutError = FString::Printf(TEXT("Material not found: '%s'"), *AssetPath);
	return Mat;
}

UMaterialExpression* FN1MCPMaterialHandler::FindExpressionByIndex(UMaterial* Mat, int32 Index)
{
	if (Index >= 0 && Index < Mat->GetExpressions().Num())
		return Mat->GetExpressions()[Index];
	return nullptr;
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::ExpressionToJson(UMaterialExpression* Expr, int32 Index)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("index"), Index);
	Obj->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
	Obj->SetStringField(TEXT("description"), Expr->GetDescription());
	Obj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
	Obj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);

	// 주요 프로퍼티 값 추출 (리플렉션)
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(Expr->GetClass()); It; ++It)
	{
		// UMaterialExpression 자체의 프로퍼티는 스킵 (상위 클래스 공통)
		if (It->GetOwnerClass() == UMaterialExpression::StaticClass())
			continue;

		void* PropAddr = It->ContainerPtrToValuePtr<void>(Expr);

		if (FFloatProperty* FP = CastField<FFloatProperty>(*It))
			Props->SetNumberField(It->GetName(), FP->GetPropertyValue(PropAddr));
		else if (FDoubleProperty* DP = CastField<FDoubleProperty>(*It))
			Props->SetNumberField(It->GetName(), DP->GetPropertyValue(PropAddr));
		else if (FIntProperty* IP = CastField<FIntProperty>(*It))
			Props->SetNumberField(It->GetName(), IP->GetPropertyValue(PropAddr));
		else if (FBoolProperty* BP = CastField<FBoolProperty>(*It))
			Props->SetBoolField(It->GetName(), BP->GetPropertyValue(PropAddr));
		else if (FStrProperty* SP = CastField<FStrProperty>(*It))
			Props->SetStringField(It->GetName(), SP->GetPropertyValue(PropAddr));
		else if (FNameProperty* NP = CastField<FNameProperty>(*It))
			Props->SetStringField(It->GetName(), NP->GetPropertyValue(PropAddr).ToString());
		else if (FStructProperty* StructP = CastField<FStructProperty>(*It))
		{
			if (StructP->Struct == TBaseStructure<FLinearColor>::Get())
			{
				FLinearColor Color = *static_cast<FLinearColor*>(PropAddr);
				TArray<TSharedPtr<FJsonValue>> Arr;
				Arr.Add(MakeShared<FJsonValueNumber>(Color.R));
				Arr.Add(MakeShared<FJsonValueNumber>(Color.G));
				Arr.Add(MakeShared<FJsonValueNumber>(Color.B));
				Arr.Add(MakeShared<FJsonValueNumber>(Color.A));
				Props->SetArrayField(It->GetName(), Arr);
			}
		}
	}
	Obj->SetObjectField(TEXT("properties"), Props);

	// 입력 연결 정보 — 리플렉션 기반 (UE 5.7에서 GetInputCount/GetInputName 미지원)
	TArray<TSharedPtr<FJsonValue>> InputsArr;
	{
		int32 ReflIdx = 0;
		for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It)
		{
			FString StructName = It->Struct->GetName();
			if (StructName.Contains(TEXT("ExpressionInput")))
			{
				FExpressionInput* Input = It->ContainerPtrToValuePtr<FExpressionInput>(Expr);
				TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
				InputObj->SetNumberField(TEXT("input_index"), ReflIdx);
				InputObj->SetStringField(TEXT("name"), It->GetName());

				if (Input && Input->Expression)
				{
					UMaterial* Mat = Cast<UMaterial>(Expr->GetOuter());
					if (Mat)
					{
						int32 SrcIdx = Mat->GetExpressions().IndexOfByKey(Input->Expression);
						InputObj->SetNumberField(TEXT("connected_to_node"), SrcIdx);
						InputObj->SetNumberField(TEXT("connected_to_output"), Input->OutputIndex);
					}
					InputObj->SetBoolField(TEXT("connected"), true);
				}
				else
				{
					InputObj->SetBoolField(TEXT("connected"), false);
				}
				InputsArr.Add(MakeShared<FJsonValueObject>(InputObj));
				ReflIdx++;
			}
		}
	}

	Obj->SetArrayField(TEXT("inputs"), InputsArr);

	return Obj;
}

void FN1MCPMaterialHandler::RegisterCommands()
{
	RegisterCommand(TEXT("create_material"),
		TEXT("머티리얼 에셋 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateMaterial(P); });

	RegisterCommand(TEXT("create_material_instance"),
		TEXT("머티리얼 인스턴스 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateMaterialInstance(P); });

	RegisterCommand(TEXT("add_material_expression"),
		TEXT("머티리얼에 노드 추가"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddMaterialExpression(P); });

	RegisterCommand(TEXT("connect_material_nodes"),
		TEXT("머티리얼 노드 연결 (인덱스 기반)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleConnectMaterialNodes(P); });

	RegisterCommand(TEXT("disconnect_material_node"),
		TEXT("머티리얼 노드 연결 해제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDisconnectMaterialNode(P); });

	RegisterCommand(TEXT("set_material_expression_param"),
		TEXT("머티리얼 노드 파라미터 수정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetMaterialExpressionParam(P); });

	RegisterCommand(TEXT("connect_to_material_output"),
		TEXT("머티리얼 최종 출력에 연결 (BaseColor, Normal 등)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleConnectToMaterialOutput(P); });

	RegisterCommand(TEXT("set_material_instance_param"),
		TEXT("머티리얼 인스턴스 파라미터 설정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetMaterialInstanceParam(P); });

	RegisterCommand(TEXT("compile_material"),
		TEXT("머티리얼 컴파일"), nullptr,
		false, false, true, 120000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCompileMaterial(P); });

	RegisterCommand(TEXT("get_material_info"),
		TEXT("머티리얼 전체 정보 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetMaterialInfo(P); });

	RegisterCommand(TEXT("set_material_property"),
		TEXT("머티리얼 속성 변경 (블렌드 모드, 셰이딩 모델 등)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetMaterialProperty(P); });
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params || !Params->TryGetStringField(TEXT("name"), Name))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'name' required"));

	FString Path = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("path"), Path);

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UObject* Asset = Factory->FactoryCreateNew(UMaterial::StaticClass(), Package,
		FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn);

	UMaterial* Mat = Cast<UMaterial>(Asset);
	if (!Mat) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create material"));

	FAssetRegistryModule::AssetCreated(Mat);
	Mat->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), Mat->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
	FString Name, ParentPath;
	if (!Params || !Params->TryGetStringField(TEXT("name"), Name))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'name' required"));
	if (!Params->TryGetStringField(TEXT("parent_material"), ParentPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'parent_material' required"));

	FString Error;
	UMaterial* Parent = FindMaterial(ParentPath, Error);
	if (!Parent) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	FString Path = TEXT("/Game/Materials");
	Params->TryGetStringField(TEXT("path"), Path);

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = Parent;

	UObject* Asset = Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), Package,
		FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn);

	UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Asset);
	if (!MIC) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create material instance"));

	FAssetRegistryModule::AssetCreated(MIC);
	MIC->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), MIC->GetPathName());
	Data->SetStringField(TEXT("parent"), Parent->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath, ExprClass;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));
	if (!Params->TryGetStringField(TEXT("expression_class"), ExprClass))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'expression_class' required"));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UClass* ExpClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ExprClass));
	if (!ExpClass)
		ExpClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.U%s"), *ExprClass));
	if (!ExpClass)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Expression class '%s' not found"), *ExprClass));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_material_expression")));

	UMaterialExpression* NewExpr = NewObject<UMaterialExpression>(Mat, ExpClass);
	if (!NewExpr)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create expression"));

	const TArray<TSharedPtr<FJsonValue>>* PosArr;
	if (Params->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
	{
		NewExpr->MaterialExpressionEditorX = static_cast<int32>((*PosArr)[0]->AsNumber());
		NewExpr->MaterialExpressionEditorY = static_cast<int32>((*PosArr)[1]->AsNumber());
	}

	Mat->GetExpressionCollection().AddExpression(NewExpr);
	Mat->AddExpressionParameter(NewExpr, Mat->EditorParameters);
	Mat->MarkPackageDirty();

	int32 Index = Mat->GetExpressions().Num() - 1;

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("node_id"), Index);
	Data->SetStringField(TEXT("class"), ExpClass->GetName());
	Data->SetStringField(TEXT("material"), Mat->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	int32 SrcIdx = static_cast<int32>(Params->GetNumberField(TEXT("source_node_id")));
	int32 SrcOut = static_cast<int32>(Params->GetNumberField(TEXT("output_index")));
	int32 DstIdx = static_cast<int32>(Params->GetNumberField(TEXT("target_node_id")));
	int32 DstIn = static_cast<int32>(Params->GetNumberField(TEXT("input_index")));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UMaterialExpression* SrcExpr = FindExpressionByIndex(Mat, SrcIdx);
	UMaterialExpression* DstExpr = FindExpressionByIndex(Mat, DstIdx);
	if (!SrcExpr) return ErrorResponse(TEXT("NODE_NOT_FOUND"), TEXT("Source expression not found"));
	if (!DstExpr) return ErrorResponse(TEXT("NODE_NOT_FOUND"), TEXT("Target expression not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: connect_material_nodes")));

	FExpressionInput* Input = DstExpr->GetInput(DstIn);

	// GetInput()이 실패하면 리플렉션으로 FExpressionInput 프로퍼티를 찾는다
	if (!Input)
	{
		int32 InputCount = 0;
		for (TFieldIterator<FStructProperty> It(DstExpr->GetClass()); It; ++It)
		{
			FString StructName = It->Struct->GetName();
			if (StructName.Contains(TEXT("ExpressionInput")))
			{
				if (InputCount == DstIn)
				{
					Input = It->ContainerPtrToValuePtr<FExpressionInput>(DstExpr);
					break;
				}
				InputCount++;
			}
		}
	}

	if (!Input)
		return ErrorResponse(TEXT("PIN_NOT_FOUND"),
			FString::Printf(TEXT("Target input index %d not found (tried GetInput + reflection)"), DstIn));

	Input->Connect(SrcOut, SrcExpr);
	Mat->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleDisconnectMaterialNode(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	int32 NodeIdx = static_cast<int32>(Params->GetNumberField(TEXT("node_id")));
	int32 InputIdx = static_cast<int32>(Params->GetNumberField(TEXT("input_index")));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UMaterialExpression* Expr = FindExpressionByIndex(Mat, NodeIdx);
	if (!Expr) return ErrorResponse(TEXT("NODE_NOT_FOUND"), TEXT("Expression not found"));

	FExpressionInput* Input = Expr->GetInput(InputIdx);

	// 리플렉션 폴백
	if (!Input)
	{
		int32 InputCount = 0;
		for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It)
		{
			FString StructName = It->Struct->GetName();
			if (StructName.Contains(TEXT("ExpressionInput")))
			{
				if (InputCount == InputIdx)
				{
					Input = It->ContainerPtrToValuePtr<FExpressionInput>(Expr);
					break;
				}
				InputCount++;
			}
		}
	}

	if (!Input) return ErrorResponse(TEXT("PIN_NOT_FOUND"), TEXT("Input not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: disconnect_material_node")));
	Input->Expression = nullptr;
	Mat->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleSetMaterialExpressionParam(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath, ParamName;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	int32 NodeIdx = static_cast<int32>(Params->GetNumberField(TEXT("node_id")));

	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'param_name' required"));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UMaterialExpression* Expr = FindExpressionByIndex(Mat, NodeIdx);
	if (!Expr) return ErrorResponse(TEXT("NODE_NOT_FOUND"), TEXT("Expression not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_material_expression_param")));
	Expr->Modify();

	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));

	// FLinearColor 특수 처리 (VectorParameter DefaultValue 등)
	FProperty* Prop = Expr->GetClass()->FindPropertyByName(FName(*ParamName));
	if (!Prop)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Property '%s' not found"), *ParamName));

	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Expr);

	// 구조체 타입 처리: FLinearColor
	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr;
			if (Params->TryGetArrayField(TEXT("value"), Arr) && Arr->Num() >= 3)
			{
				FLinearColor Color;
				Color.R = static_cast<float>((*Arr)[0]->AsNumber());
				Color.G = static_cast<float>((*Arr)[1]->AsNumber());
				Color.B = static_cast<float>((*Arr)[2]->AsNumber());
				Color.A = Arr->Num() >= 4 ? static_cast<float>((*Arr)[3]->AsNumber()) : 1.0f;
				*static_cast<FLinearColor*>(PropAddr) = Color;
			}
			else
			{
				return ErrorResponse(TEXT("INVALID_PARAMS"),
					TEXT("FLinearColor requires array [R, G, B] or [R, G, B, A]"));
			}
		}
		else if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr;
			if (Params->TryGetArrayField(TEXT("value"), Arr) && Arr->Num() >= 3)
			{
				FVector Vec((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
				*static_cast<FVector*>(PropAddr) = Vec;
			}
		}
		else
		{
			return ErrorResponse(TEXT("INVALID_PARAMS"),
				FString::Printf(TEXT("Unsupported struct type for '%s'"), *ParamName));
		}
	}
	else if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
		FP->SetPropertyValue(PropAddr, static_cast<float>(Value->AsNumber()));
	else if (FDoubleProperty* DP = CastField<FDoubleProperty>(Prop))
		DP->SetPropertyValue(PropAddr, Value->AsNumber());
	else if (FIntProperty* IP = CastField<FIntProperty>(Prop))
		IP->SetPropertyValue(PropAddr, static_cast<int32>(Value->AsNumber()));
	else if (FStrProperty* SP = CastField<FStrProperty>(Prop))
		SP->SetPropertyValue(PropAddr, Value->AsString());
	else if (FNameProperty* NP = CastField<FNameProperty>(Prop))
		NP->SetPropertyValue(PropAddr, FName(*Value->AsString()));
	else if (FBoolProperty* BoolP = CastField<FBoolProperty>(Prop))
		BoolP->SetPropertyValue(PropAddr, Value->AsBool());
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
			FString::Printf(TEXT("Unsupported property type for '%s'"), *ParamName));

	Mat->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleConnectToMaterialOutput(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath, MatProp;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	int32 NodeIdx = static_cast<int32>(Params->GetNumberField(TEXT("node_id")));
	int32 OutputIdx = static_cast<int32>(Params->GetNumberField(TEXT("output_index")));

	if (!Params->TryGetStringField(TEXT("material_property"), MatProp))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_property' required"));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UMaterialExpression* Expr = FindExpressionByIndex(Mat, NodeIdx);
	if (!Expr) return ErrorResponse(TEXT("NODE_NOT_FOUND"), TEXT("Expression not found"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: connect_to_material_output")));

	FExpressionInput* TargetInput = nullptr;
	if (MatProp == TEXT("BaseColor")) TargetInput = &Mat->GetEditorOnlyData()->BaseColor;
	else if (MatProp == TEXT("Metallic")) TargetInput = &Mat->GetEditorOnlyData()->Metallic;
	else if (MatProp == TEXT("Specular")) TargetInput = &Mat->GetEditorOnlyData()->Specular;
	else if (MatProp == TEXT("Roughness")) TargetInput = &Mat->GetEditorOnlyData()->Roughness;
	else if (MatProp == TEXT("EmissiveColor")) TargetInput = &Mat->GetEditorOnlyData()->EmissiveColor;
	else if (MatProp == TEXT("Normal")) TargetInput = &Mat->GetEditorOnlyData()->Normal;
	else if (MatProp == TEXT("Opacity")) TargetInput = &Mat->GetEditorOnlyData()->Opacity;
	else if (MatProp == TEXT("OpacityMask")) TargetInput = &Mat->GetEditorOnlyData()->OpacityMask;
	else if (MatProp == TEXT("WorldPositionOffset")) TargetInput = &Mat->GetEditorOnlyData()->WorldPositionOffset;
	else if (MatProp == TEXT("AmbientOcclusion")) TargetInput = &Mat->GetEditorOnlyData()->AmbientOcclusion;

	if (!TargetInput)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Unknown material property: '%s'"), *MatProp));

	TargetInput->Connect(OutputIdx, Expr);
	Mat->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("property"), MatProp);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleSetMaterialInstanceParam(const TSharedPtr<FJsonObject>& Params)
{
	FString InstPath, ParamName, ParamType;
	if (!Params || !Params->TryGetStringField(TEXT("instance_path"), InstPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'instance_path' required"));
	if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'param_name' required"));
	if (!Params->TryGetStringField(TEXT("param_type"), ParamType))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'param_type' required"));

	FString AssetPath = InstPath.StartsWith(TEXT("/")) ? InstPath : TEXT("/Game/Materials/") + InstPath;
	UMaterialInstanceConstant* MIC = LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
	if (!MIC)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Material instance not found: '%s'"), *AssetPath));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_material_instance_param")));
	MIC->Modify();

	if (ParamType == TEXT("Scalar"))
	{
		float Value = static_cast<float>(Params->GetNumberField(TEXT("value")));
		MIC->SetScalarParameterValueEditorOnly(FName(*ParamName), Value);
	}
	else if (ParamType == TEXT("Vector"))
	{
		FLinearColor Color(0, 0, 0, 1);
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("value"), Arr) && Arr->Num() >= 3)
		{
			Color.R = static_cast<float>((*Arr)[0]->AsNumber());
			Color.G = static_cast<float>((*Arr)[1]->AsNumber());
			Color.B = static_cast<float>((*Arr)[2]->AsNumber());
			Color.A = Arr->Num() >= 4 ? static_cast<float>((*Arr)[3]->AsNumber()) : 1.0f;
		}
		MIC->SetVectorParameterValueEditorOnly(FName(*ParamName), Color);
	}
	else if (ParamType == TEXT("Texture"))
	{
		FString TexPath = Params->GetStringField(TEXT("value"));
		UTexture* Tex = LoadObject<UTexture>(nullptr, *TexPath);
		if (!Tex)
			return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
				FString::Printf(TEXT("Texture not found: '%s'"), *TexPath));
		MIC->SetTextureParameterValueEditorOnly(FName(*ParamName), Tex);
	}
	else
	{
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Unknown param_type: '%s' (use Scalar/Vector/Texture)"), *ParamType));
	}

	MIC->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("material"), Mat->GetPathName());
	Data->SetBoolField(TEXT("compiled"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	TArray<TSharedPtr<FJsonValue>> ExprArray;
	for (int32 i = 0; i < Mat->GetExpressions().Num(); ++i)
	{
		ExprArray.Add(MakeShared<FJsonValueObject>(ExpressionToJson(Mat->GetExpressions()[i], i)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), Mat->GetPathName());
	Data->SetArrayField(TEXT("expressions"), ExprArray);
	Data->SetNumberField(TEXT("expression_count"), ExprArray.Num());

	FString BlendMode;
	switch (Mat->BlendMode)
	{
	case BLEND_Opaque: BlendMode = TEXT("Opaque"); break;
	case BLEND_Masked: BlendMode = TEXT("Masked"); break;
	case BLEND_Translucent: BlendMode = TEXT("Translucent"); break;
	case BLEND_Additive: BlendMode = TEXT("Additive"); break;
	default: BlendMode = TEXT("Other"); break;
	}
	Data->SetStringField(TEXT("blend_mode"), BlendMode);

	// 머티리얼 최종 출력 연결 정보
	auto GetOutputConnection = [&](FExpressionInput& Input, const FString& Name) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> Conn = MakeShared<FJsonObject>();
		Conn->SetStringField(TEXT("property"), Name);
		if (Input.Expression)
		{
			int32 SrcIdx = Mat->GetExpressions().IndexOfByKey(Input.Expression);
			Conn->SetNumberField(TEXT("connected_to_node"), SrcIdx);
			Conn->SetNumberField(TEXT("connected_to_output"), Input.OutputIndex);
			Conn->SetBoolField(TEXT("connected"), true);
		}
		else
		{
			Conn->SetBoolField(TEXT("connected"), false);
		}
		return Conn;
	};

	TArray<TSharedPtr<FJsonValue>> OutputConns;
	auto* EditorData = Mat->GetEditorOnlyData();
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->BaseColor, TEXT("BaseColor"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->Metallic, TEXT("Metallic"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->Specular, TEXT("Specular"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->Roughness, TEXT("Roughness"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->EmissiveColor, TEXT("EmissiveColor"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->Normal, TEXT("Normal"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->Opacity, TEXT("Opacity"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->OpacityMask, TEXT("OpacityMask"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->WorldPositionOffset, TEXT("WorldPositionOffset"))));
	OutputConns.Add(MakeShared<FJsonValueObject>(GetOutputConnection(EditorData->AmbientOcclusion, TEXT("AmbientOcclusion"))));
	Data->SetArrayField(TEXT("output_connections"), OutputConns);

	// ShadingModel
	FString ShadingModel;
	FMaterialShadingModelField SMField = Mat->GetShadingModels();
	if (SMField.HasShadingModel(MSM_Unlit)) ShadingModel = TEXT("Unlit");
	else if (SMField.HasShadingModel(MSM_DefaultLit)) ShadingModel = TEXT("DefaultLit");
	else if (SMField.HasShadingModel(MSM_Subsurface)) ShadingModel = TEXT("Subsurface");
	else if (SMField.HasShadingModel(MSM_ClearCoat)) ShadingModel = TEXT("ClearCoat");
	else ShadingModel = TEXT("Other");
	Data->SetStringField(TEXT("shading_model"), ShadingModel);

	Data->SetBoolField(TEXT("two_sided"), Mat->TwoSided);

	// MaterialDomain
	FString Domain;
	switch (Mat->MaterialDomain)
	{
	case MD_Surface: Domain = TEXT("Surface"); break;
	case MD_DeferredDecal: Domain = TEXT("DeferredDecal"); break;
	case MD_LightFunction: Domain = TEXT("LightFunction"); break;
	case MD_PostProcess: Domain = TEXT("PostProcess"); break;
	case MD_UI: Domain = TEXT("UI"); break;
	default: Domain = TEXT("Other"); break;
	}
	Data->SetStringField(TEXT("material_domain"), Domain);

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPMaterialHandler::HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString MatPath, PropName;
	if (!Params || !Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));
	if (!Params->TryGetStringField(TEXT("property"), PropName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'property' required"));

	FString Error;
	UMaterial* Mat = FindMaterial(MatPath, Error);
	if (!Mat) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_material_property")));
	Mat->Modify();

	FString Value = Params->GetStringField(TEXT("value"));

	if (PropName == TEXT("BlendMode"))
	{
		if (Value == TEXT("Opaque")) Mat->BlendMode = BLEND_Opaque;
		else if (Value == TEXT("Masked")) Mat->BlendMode = BLEND_Masked;
		else if (Value == TEXT("Translucent")) Mat->BlendMode = BLEND_Translucent;
		else if (Value == TEXT("Additive")) Mat->BlendMode = BLEND_Additive;
		else if (Value == TEXT("Modulate")) Mat->BlendMode = BLEND_Modulate;
		else if (Value == TEXT("AlphaComposite")) Mat->BlendMode = BLEND_AlphaComposite;
	}
	else if (PropName == TEXT("ShadingModel"))
	{
		if (Value == TEXT("DefaultLit") || Value == TEXT("Default Lit"))
			Mat->SetShadingModel(MSM_DefaultLit);
		else if (Value == TEXT("Unlit"))
			Mat->SetShadingModel(MSM_Unlit);
		else if (Value == TEXT("Subsurface"))
			Mat->SetShadingModel(MSM_Subsurface);
		else if (Value == TEXT("PreintegratedSkin"))
			Mat->SetShadingModel(MSM_PreintegratedSkin);
		else if (Value == TEXT("ClearCoat"))
			Mat->SetShadingModel(MSM_ClearCoat);
		else if (Value == TEXT("SubsurfaceProfile"))
			Mat->SetShadingModel(MSM_SubsurfaceProfile);
		else if (Value == TEXT("TwoSidedFoliage"))
			Mat->SetShadingModel(MSM_TwoSidedFoliage);
		else if (Value == TEXT("Hair"))
			Mat->SetShadingModel(MSM_Hair);
		else if (Value == TEXT("Cloth"))
			Mat->SetShadingModel(MSM_Cloth);
		else if (Value == TEXT("Eye"))
			Mat->SetShadingModel(MSM_Eye);
		else if (Value == TEXT("SingleLayerWater"))
			Mat->SetShadingModel(MSM_SingleLayerWater);
		else
			return ErrorResponse(TEXT("INVALID_PARAMS"),
				FString::Printf(TEXT("Unknown ShadingModel: '%s'"), *Value));
	}
	else if (PropName == TEXT("MaterialDomain"))
	{
		if (Value == TEXT("Surface")) Mat->MaterialDomain = MD_Surface;
		else if (Value == TEXT("DeferredDecal")) Mat->MaterialDomain = MD_DeferredDecal;
		else if (Value == TEXT("LightFunction")) Mat->MaterialDomain = MD_LightFunction;
		else if (Value == TEXT("PostProcess")) Mat->MaterialDomain = MD_PostProcess;
		else if (Value == TEXT("UI")) Mat->MaterialDomain = MD_UI;
	}
	else if (PropName == TEXT("TwoSided"))
	{
		Mat->TwoSided = Value == TEXT("true") || Value == TEXT("1");
	}
	else if (PropName == TEXT("Wireframe"))
	{
		Mat->Wireframe = Value == TEXT("true") || Value == TEXT("1");
	}
	else if (PropName == TEXT("OpacityMaskClipValue"))
	{
		Mat->OpacityMaskClipValue = FCString::Atof(*Value);
	}
	else
	{
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Unsupported material property: '%s' (supported: BlendMode, ShadingModel, MaterialDomain, TwoSided, Wireframe, OpacityMaskClipValue)"), *PropName));
	}

	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	Mat->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}
