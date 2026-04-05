#include "Handlers/N1MCPDataHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Curves/RichCurve.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/DataTableFactory.h"
#include "Factories/CurveFactory.h"
#include "UObject/SavePackage.h"

FN1MCPDataHandler::FN1MCPDataHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("data"))
{
}

void FN1MCPDataHandler::RegisterCommands()
{
	RegisterCommand(TEXT("create_data_table"),
		TEXT("DataTable 에셋 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateDataTable(P); });

	RegisterCommand(TEXT("get_data_table_rows"),
		TEXT("DataTable 행 데이터 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetDataTableRows(P); });

	RegisterCommand(TEXT("get_data_table_row_names"),
		TEXT("DataTable 행 이름 목록"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetDataTableRowNames(P); });

	RegisterCommand(TEXT("add_data_table_row"),
		TEXT("DataTable에 행 추가"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddDataTableRow(P); });

	RegisterCommand(TEXT("remove_data_table_row"),
		TEXT("DataTable에서 행 제거"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleRemoveDataTableRow(P); });

	RegisterCommand(TEXT("get_curve_table_info"),
		TEXT("CurveTable 정보 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetCurveTableInfo(P); });

	RegisterCommand(TEXT("get_data_asset_properties"),
		TEXT("DataAsset 속성 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetDataAssetProperties(P); });

	// Curve (CurveFloat, CurveLinearColor, CurveVector)
	RegisterCommand(TEXT("create_curve"),
		TEXT("커브 에셋 생성 (CurveFloat, CurveLinearColor, CurveVector)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateCurve(P); });

	RegisterCommand(TEXT("get_curve_keys"),
		TEXT("커브 키 데이터 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetCurveKeys(P); });

	RegisterCommand(TEXT("set_curve_keys"),
		TEXT("커브에 키 데이터 일괄 설정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetCurveKeys(P); });
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params)
{
	FString Name, RowStructName;
	if (!Params || !Params->TryGetStringField(TEXT("name"), Name))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'name' required"));
	if (!Params->TryGetStringField(TEXT("row_struct"), RowStructName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'row_struct' required"));

	UScriptStruct* RowStruct = FindObject<UScriptStruct>(nullptr, *RowStructName);
	if (!RowStruct)
	{
		FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *RowStructName);
		RowStruct = FindObject<UScriptStruct>(nullptr, *FullPath);
	}
	if (!RowStruct)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Row struct '%s' not found"), *RowStructName));

	FString Path = TEXT("/Game/Data");
	Params->TryGetStringField(TEXT("path"), Path);

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = RowStruct;

	UObject* Asset = Factory->FactoryCreateNew(UDataTable::StaticClass(), Package,
		FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn);

	UDataTable* DT = Cast<UDataTable>(Asset);
	if (!DT)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create data table"));

	FAssetRegistryModule::AssetCreated(DT);
	DT->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), DT->GetPathName());
	Data->SetStringField(TEXT("row_struct"), RowStruct->GetName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleGetDataTableRows(const TSharedPtr<FJsonObject>& Params)
{
	FString DTPath;
	if (!Params || !Params->TryGetStringField(TEXT("data_table_path"), DTPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'data_table_path' required"));

	UDataTable* DT = LoadObject<UDataTable>(nullptr, *DTPath);
	if (!DT)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("DataTable not found: '%s'"), *DTPath));

	TArray<TSharedPtr<FJsonValue>> RowArray;
	FString ContextString;
	TArray<FName> RowNames = DT->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		uint8* RowData = DT->FindRowUnchecked(RowName);
		if (!RowData) continue;

		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("row_name"), RowName.ToString());

		// 각 프로퍼티를 JSON으로 직렬화
		const UScriptStruct* RowStruct = DT->GetRowStruct();
		if (RowStruct)
		{
			for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
			{
				FProperty* Prop = *It;
				FString ValueStr;
				Prop->ExportTextItem_Direct(ValueStr, Prop->ContainerPtrToValuePtr<void>(RowData),
					nullptr, nullptr, PPF_None);
				RowObj->SetStringField(Prop->GetName(), ValueStr);
			}
		}

		RowArray.Add(MakeShared<FJsonValueObject>(RowObj));
	}

	TSharedPtr<FJsonObject> Data = FN1MCPCommandRegistry::ApplyPagination(RowArray, Params);
	Data->SetStringField(TEXT("data_table"), DTPath);
	Data->SetStringField(TEXT("row_struct"), DT->GetRowStruct() ? DT->GetRowStruct()->GetName() : TEXT("Unknown"));
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleGetDataTableRowNames(const TSharedPtr<FJsonObject>& Params)
{
	FString DTPath;
	if (!Params || !Params->TryGetStringField(TEXT("data_table_path"), DTPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'data_table_path' required"));

	UDataTable* DT = LoadObject<UDataTable>(nullptr, *DTPath);
	if (!DT)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("DataTable not found: '%s'"), *DTPath));

	TArray<TSharedPtr<FJsonValue>> Names;
	for (const FName& RowName : DT->GetRowNames())
	{
		Names.Add(MakeShared<FJsonValueString>(RowName.ToString()));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("row_names"), Names);
	Data->SetNumberField(TEXT("count"), Names.Num());
	Data->SetStringField(TEXT("data_table"), DTPath);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params)
{
	FString DTPath, RowName;
	if (!Params || !Params->TryGetStringField(TEXT("data_table_path"), DTPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'data_table_path' required"));
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'row_name' required"));

	UDataTable* DT = LoadObject<UDataTable>(nullptr, *DTPath);
	if (!DT)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("DataTable not found: '%s'"), *DTPath));

	if (DT->FindRowUnchecked(FName(*RowName)))
		return ErrorResponse(TEXT("ASSET_ALREADY_EXISTS"),
			FString::Printf(TEXT("Row '%s' already exists"), *RowName));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_data_table_row")));
	DT->Modify();

	// 빈 행 추가 (JSON 문자열로)
	FString RowJson = TEXT("{}");
	const TSharedPtr<FJsonObject>* RowValues;
	if (Params->TryGetObjectField(TEXT("values"), RowValues))
	{
		FString JsonStr;
		auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr);
		FJsonSerializer::Serialize((*RowValues).ToSharedRef(), Writer);
		Writer->Close();
		RowJson = JsonStr;
	}

	TArray<FString> Errors;
	FString EmptyJson = FString::Printf(TEXT("[\n{\"Name\":\"%s\"}\n]"), *RowName);

	// AddRow via DataTableEditorUtils
	// 빈 행 생성: 구조체 메모리를 할당하고 AddRow
	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!RowStruct)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("DataTable has no row struct"));

	uint8* RowMemory = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
	RowStruct->InitializeStruct(RowMemory);
	DT->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(RowMemory));
	RowStruct->DestroyStruct(RowMemory);
	FMemory::Free(RowMemory);
	DT->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("row_name"), RowName);
	Data->SetStringField(TEXT("data_table"), DTPath);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleRemoveDataTableRow(const TSharedPtr<FJsonObject>& Params)
{
	FString DTPath, RowName;
	if (!Params || !Params->TryGetStringField(TEXT("data_table_path"), DTPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'data_table_path' required"));
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'row_name' required"));

	UDataTable* DT = LoadObject<UDataTable>(nullptr, *DTPath);
	if (!DT)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("DataTable not found: '%s'"), *DTPath));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: remove_data_table_row")));
	DT->Modify();
	DT->RemoveRow(FName(*RowName));
	DT->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("removed"), RowName);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleGetCurveTableInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString CTPath;
	if (!Params || !Params->TryGetStringField(TEXT("curve_table_path"), CTPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'curve_table_path' required"));

	UCurveTable* CT = LoadObject<UCurveTable>(nullptr, *CTPath);
	if (!CT)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("CurveTable not found: '%s'"), *CTPath));

	TArray<TSharedPtr<FJsonValue>> RowArray;
	const TMap<FName, FRealCurve*>& RowMap = CT->GetRowMap();
	for (const auto& Pair : RowMap)
	{
		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("name"), Pair.Key.ToString());
		if (Pair.Value)
			RowObj->SetNumberField(TEXT("num_keys"), Pair.Value->GetNumKeys());
		RowArray.Add(MakeShared<FJsonValueObject>(RowObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), CT->GetPathName());
	Data->SetArrayField(TEXT("curves"), RowArray);
	Data->SetNumberField(TEXT("count"), RowArray.Num());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleGetDataAssetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params || !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'asset_path' required"));

	UDataAsset* DA = LoadObject<UDataAsset>(nullptr, *AssetPath);
	if (!DA)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("DataAsset not found: '%s'"), *AssetPath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), DA->GetPathName());
	Data->SetStringField(TEXT("class"), DA->GetClass()->GetName());

	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (TFieldIterator<FProperty> It(DA->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop->GetOwnerClass() == UObject::StaticClass()) continue;
		if (Prop->GetOwnerClass() == UDataAsset::StaticClass()) continue;

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Prop->GetName());
		PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());

		FString ValueStr;
		Prop->ExportTextItem_Direct(ValueStr,
			Prop->ContainerPtrToValuePtr<void>(DA),
			nullptr, DA, PPF_None);
		PropObj->SetStringField(TEXT("value"), ValueStr);

		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}
	Data->SetArrayField(TEXT("properties"), PropsArray);

	return SuccessResponse(Data);
}

// ── 헬퍼: FRichCurve 키를 JSON 배열로 ──
static TArray<TSharedPtr<FJsonValue>> RichCurveToJsonKeys(const FRichCurve& Curve)
{
	TArray<TSharedPtr<FJsonValue>> Keys;
	for (auto It = Curve.GetKeyIterator(); It; ++It)
	{
		TSharedPtr<FJsonObject> K = MakeShared<FJsonObject>();
		K->SetNumberField(TEXT("time"), It->Time);
		K->SetNumberField(TEXT("value"), It->Value);

		FString InterpStr;
		switch (It->InterpMode)
		{
		case RCIM_Linear: InterpStr = TEXT("Linear"); break;
		case RCIM_Constant: InterpStr = TEXT("Constant"); break;
		case RCIM_Cubic: InterpStr = TEXT("Cubic"); break;
		default: InterpStr = TEXT("Linear"); break;
		}
		K->SetStringField(TEXT("interp"), InterpStr);
		K->SetNumberField(TEXT("arrive_tangent"), It->ArriveTangent);
		K->SetNumberField(TEXT("leave_tangent"), It->LeaveTangent);

		Keys.Add(MakeShared<FJsonValueObject>(K));
	}
	return Keys;
}

// ── 헬퍼: JSON 배열에서 FRichCurve에 키 설정 ──
static void JsonKeysToRichCurve(FRichCurve& Curve, const TArray<TSharedPtr<FJsonValue>>& KeysArr)
{
	Curve.Reset();
	for (const auto& KeyVal : KeysArr)
	{
		const TSharedPtr<FJsonObject>& K = KeyVal->AsObject();
		if (!K.IsValid()) continue;

		float Time = static_cast<float>(K->GetNumberField(TEXT("time")));
		float Value = static_cast<float>(K->GetNumberField(TEXT("value")));

		FKeyHandle Handle = Curve.AddKey(Time, Value);

		FString InterpStr;
		if (K->TryGetStringField(TEXT("interp"), InterpStr))
		{
			if (InterpStr == TEXT("Linear")) Curve.SetKeyInterpMode(Handle, RCIM_Linear);
			else if (InterpStr == TEXT("Constant")) Curve.SetKeyInterpMode(Handle, RCIM_Constant);
			else if (InterpStr == TEXT("Cubic")) Curve.SetKeyInterpMode(Handle, RCIM_Cubic);
		}

		if (K->HasField(TEXT("arrive_tangent")))
		{
			FRichCurveKey& Key = Curve.GetKey(Handle);
			Key.ArriveTangent = static_cast<float>(K->GetNumberField(TEXT("arrive_tangent")));
			Key.LeaveTangent = static_cast<float>(K->GetNumberField(TEXT("leave_tangent")));
		}
	}
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleCreateCurve(const TSharedPtr<FJsonObject>& Params)
{
	FString Name, CurveType;
	if (!Params || !Params->TryGetStringField(TEXT("name"), Name))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'name' required"));
	if (!Params->TryGetStringField(TEXT("curve_type"), CurveType))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'curve_type' required (CurveFloat, CurveLinearColor, CurveVector)"));

	FString Path = TEXT("/Game/Data");
	Params->TryGetStringField(TEXT("path"), Path);

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);

	UObject* Asset = nullptr;

	if (CurveType == TEXT("CurveFloat") || CurveType == TEXT("Float"))
	{
		UCurveFloat* Curve = NewObject<UCurveFloat>(Package, FName(*Name), RF_Public | RF_Standalone);
		Asset = Curve;
	}
	else if (CurveType == TEXT("CurveLinearColor") || CurveType == TEXT("LinearColor") || CurveType == TEXT("Color"))
	{
		UCurveLinearColor* Curve = NewObject<UCurveLinearColor>(Package, FName(*Name), RF_Public | RF_Standalone);
		Asset = Curve;
	}
	else if (CurveType == TEXT("CurveVector") || CurveType == TEXT("Vector"))
	{
		UCurveVector* Curve = NewObject<UCurveVector>(Package, FName(*Name), RF_Public | RF_Standalone);
		Asset = Curve;
	}
	else
	{
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Unknown curve_type: '%s'"), *CurveType));
	}

	if (!Asset) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create curve"));

	FAssetRegistryModule::AssetCreated(Asset);
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), Asset->GetPathName());
	Data->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleGetCurveKeys(const TSharedPtr<FJsonObject>& Params)
{
	FString CurvePath;
	if (!Params || !Params->TryGetStringField(TEXT("curve_path"), CurvePath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'curve_path' required"));

	UObject* Obj = LoadObject<UObject>(nullptr, *CurvePath);
	if (!Obj)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Curve not found: '%s'"), *CurvePath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), Obj->GetPathName());
	Data->SetStringField(TEXT("class"), Obj->GetClass()->GetName());

	if (UCurveFloat* CF = Cast<UCurveFloat>(Obj))
	{
		Data->SetArrayField(TEXT("keys"), RichCurveToJsonKeys(CF->FloatCurve));
		Data->SetNumberField(TEXT("num_keys"), CF->FloatCurve.GetNumKeys());
	}
	else if (UCurveLinearColor* CLC = Cast<UCurveLinearColor>(Obj))
	{
		for (int32 i = 0; i < 4; ++i)
		{
			FString ChannelName;
			switch (i)
			{
			case 0: ChannelName = TEXT("r"); break;
			case 1: ChannelName = TEXT("g"); break;
			case 2: ChannelName = TEXT("b"); break;
			case 3: ChannelName = TEXT("a"); break;
			}
			Data->SetArrayField(ChannelName, RichCurveToJsonKeys(CLC->FloatCurves[i]));
		}
	}
	else if (UCurveVector* CV = Cast<UCurveVector>(Obj))
	{
		for (int32 i = 0; i < 3; ++i)
		{
			FString ChannelName;
			switch (i)
			{
			case 0: ChannelName = TEXT("x"); break;
			case 1: ChannelName = TEXT("y"); break;
			case 2: ChannelName = TEXT("z"); break;
			}
			Data->SetArrayField(ChannelName, RichCurveToJsonKeys(CV->FloatCurves[i]));
		}
	}
	else
	{
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			TEXT("Object is not a CurveFloat, CurveLinearColor, or CurveVector"));
	}

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPDataHandler::HandleSetCurveKeys(const TSharedPtr<FJsonObject>& Params)
{
	FString CurvePath;
	if (!Params || !Params->TryGetStringField(TEXT("curve_path"), CurvePath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'curve_path' required"));

	UObject* Obj = LoadObject<UObject>(nullptr, *CurvePath);
	if (!Obj)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Curve not found: '%s'"), *CurvePath));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_curve_keys")));
	Obj->Modify();

	if (UCurveFloat* CF = Cast<UCurveFloat>(Obj))
	{
		const TArray<TSharedPtr<FJsonValue>>* KeysArr;
		if (Params->TryGetArrayField(TEXT("keys"), KeysArr))
		{
			JsonKeysToRichCurve(CF->FloatCurve, *KeysArr);
		}
	}
	else if (UCurveLinearColor* CLC = Cast<UCurveLinearColor>(Obj))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("r"), Arr)) JsonKeysToRichCurve(CLC->FloatCurves[0], *Arr);
		if (Params->TryGetArrayField(TEXT("g"), Arr)) JsonKeysToRichCurve(CLC->FloatCurves[1], *Arr);
		if (Params->TryGetArrayField(TEXT("b"), Arr)) JsonKeysToRichCurve(CLC->FloatCurves[2], *Arr);
		if (Params->TryGetArrayField(TEXT("a"), Arr)) JsonKeysToRichCurve(CLC->FloatCurves[3], *Arr);
	}
	else if (UCurveVector* CV = Cast<UCurveVector>(Obj))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("x"), Arr)) JsonKeysToRichCurve(CV->FloatCurves[0], *Arr);
		if (Params->TryGetArrayField(TEXT("y"), Arr)) JsonKeysToRichCurve(CV->FloatCurves[1], *Arr);
		if (Params->TryGetArrayField(TEXT("z"), Arr)) JsonKeysToRichCurve(CV->FloatCurves[2], *Arr);
	}
	else
	{
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			TEXT("Object is not a CurveFloat, CurveLinearColor, or CurveVector"));
	}

	Obj->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("curve"), Obj->GetPathName());
	return SuccessResponse(Data);
}
