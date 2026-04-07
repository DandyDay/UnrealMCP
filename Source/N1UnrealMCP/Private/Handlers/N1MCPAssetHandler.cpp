#include "Handlers/N1MCPAssetHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "AutomatedAssetImportData.h"
#include "AssetImportTask.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "Factories/Factory.h"
#include "Factories/DataTableFactory.h"

FN1MCPAssetHandler::FN1MCPAssetHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("asset"))
{
}

void FN1MCPAssetHandler::RegisterCommands()
{
	RegisterCommand(TEXT("create_asset"),
		TEXT("범용 에셋 생성 (InputAction, DataTable, CurveFloat 등)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateAsset(P); });

	RegisterCommand(TEXT("find_assets"),
		TEXT("AssetRegistry로 에셋 검색"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleFindAssets(P); });

	RegisterCommand(TEXT("get_asset_info"),
		TEXT("에셋 상세 정보 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetAssetInfo(P); });

	RegisterCommand(TEXT("get_asset_references"),
		TEXT("에셋 의존성/역참조 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetAssetReferences(P); });

	RegisterCommand(TEXT("import_asset"),
		TEXT("외부 파일 임포트 (텍스처, FBX 등)"), nullptr,
		true, false, true, 120000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleImportAsset(P); });

	RegisterCommand(TEXT("rename_asset"),
		TEXT("에셋 이름 변경 (리다이렉터 자동)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleRenameAsset(P); });

	RegisterCommand(TEXT("move_asset"),
		TEXT("에셋 이동"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleMoveAsset(P); });

	RegisterCommand(TEXT("delete_asset"),
		TEXT("에셋 삭제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDeleteAsset(P); });

	RegisterCommand(TEXT("duplicate_asset"),
		TEXT("에셋 복제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDuplicateAsset(P); });
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleFindAssets(const TSharedPtr<FJsonObject>& Params)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;

	FString SearchPath, ClassFilter, NamePattern;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("search_path"), SearchPath);
		Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
		Params->TryGetStringField(TEXT("name_pattern"), NamePattern);

		if (!SearchPath.IsEmpty())
			Filter.PackagePaths.Add(FName(*SearchPath));
	}

	if (Filter.PackagePaths.Num() == 0)
		Filter.PackagePaths.Add(FName(TEXT("/Game")));

	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> AllAssets;
	for (const FAssetData& Asset : Assets)
	{
		// class_filter: Contains 매칭 (짧은 이름 OK — CurveFloat, Blueprint 등)
		if (!ClassFilter.IsEmpty() &&
			!Asset.AssetClassPath.GetAssetName().ToString().Contains(ClassFilter))
			continue;

		if (!NamePattern.IsEmpty() && !Asset.AssetName.ToString().Contains(NamePattern))
			continue;

		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("asset_name"), Asset.AssetName.ToString());
		Obj->SetStringField(TEXT("asset_path"), Asset.GetObjectPathString());
		Obj->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());
		Obj->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
		AllAssets.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Data = FN1MCPCommandRegistry::ApplyPagination(AllAssets, Params);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params || !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'asset_path' required"));

	// 짧은 경로 자동 보정: /Game/Path/Name → /Game/Path/Name.Name
	if (!AssetPath.Contains(TEXT(".")))
	{
		FString AssetName = FPackageName::GetShortName(AssetPath);
		AssetPath = AssetPath + TEXT(".") + AssetName;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));

	if (!AssetData.IsValid())
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
	Data->SetStringField(TEXT("asset_path"), AssetData.GetObjectPathString());
	Data->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
	Data->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
	Data->SetNumberField(TEXT("disk_size"), static_cast<double>(AssetData.GetPackage() ? AssetData.GetPackage()->GetFileSize() : 0));

	// 레퍼런스 수
	TArray<FName> Referencers;
	AssetRegistry.GetReferencers(AssetData.PackageName, Referencers);
	Data->SetNumberField(TEXT("referencer_count"), Referencers.Num());

	TArray<FName> Dependencies;
	AssetRegistry.GetDependencies(AssetData.PackageName, Dependencies);
	Data->SetNumberField(TEXT("dependency_count"), Dependencies.Num());

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleGetAssetReferences(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, Direction;
	if (!Params || !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'asset_path' required"));
	if (!Params->TryGetStringField(TEXT("direction"), Direction))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'direction' required (dependencies/referencers)"));

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (!AssetData.IsValid())
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));

	TArray<FName> Results;
	if (Direction == TEXT("dependencies"))
		AssetRegistry.GetDependencies(AssetData.PackageName, Results);
	else if (Direction == TEXT("referencers"))
		AssetRegistry.GetReferencers(AssetData.PackageName, Results);
	else
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			TEXT("direction must be 'dependencies' or 'referencers'"));

	TArray<TSharedPtr<FJsonValue>> AllRefs;
	for (const FName& Ref : Results)
	{
		AllRefs.Add(MakeShared<FJsonValueString>(Ref.ToString()));
	}

	TSharedPtr<FJsonObject> Data = FN1MCPCommandRegistry::ApplyPagination(AllRefs, Params);
	Data->SetStringField(TEXT("asset"), AssetPath);
	Data->SetStringField(TEXT("direction"), Direction);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString SourceFile, DestPath;
	if (!Params || !Params->TryGetStringField(TEXT("source_file"), SourceFile))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'source_file' required"));
	if (!Params->TryGetStringField(TEXT("destination_path"), DestPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'destination_path' required"));

	if (!FPaths::FileExists(SourceFile))
		return ErrorResponse(TEXT("IMPORT_FAILED"),
			FString::Printf(TEXT("Source file not found: '%s'"), *SourceFile));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// UAssetImportTask 사용: Interchange TaskGraph 재귀 방지
	UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
	ImportTask->Filename = SourceFile;
	ImportTask->DestinationPath = DestPath;
	ImportTask->bReplaceExisting = true;
	ImportTask->bAutomated = true;
	ImportTask->bSave = false;

	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(ImportTask);
	AssetTools.ImportAssetTasks(Tasks);

	TArray<UObject*> ImportedAssets = ImportTask->GetObjects();

	if (ImportedAssets.Num() == 0)
		return ErrorResponse(TEXT("IMPORT_FAILED"),
			FString::Printf(TEXT("Failed to import '%s'"), *SourceFile));

	TArray<TSharedPtr<FJsonValue>> Imported;
	for (UObject* Asset : ImportedAssets)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("asset_path"), Asset->GetPathName());
		Obj->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
		Imported.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("imported"), Imported);
	Data->SetNumberField(TEXT("count"), Imported.Num());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, NewName;
	if (!Params || !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'asset_path' required"));
	if (!Params->TryGetStringField(TEXT("new_name"), NewName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'new_name' required"));

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString PackagePath = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
	FString NewPackagePath = PackagePath / NewName;

	TArray<FAssetRenameData> RenameData;
	RenameData.Add(FAssetRenameData(Asset, PackagePath, NewName));

	bool bSuccess = AssetTools.RenameAssets(RenameData);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), bSuccess);
	Data->SetStringField(TEXT("new_path"), NewPackagePath + TEXT(".") + NewName);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleMoveAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, NewPath;
	if (!Params || !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'asset_path' required"));
	if (!Params->TryGetStringField(TEXT("new_path"), NewPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'new_path' required"));

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString AssetName = Asset->GetName();
	TArray<FAssetRenameData> RenameData;
	RenameData.Add(FAssetRenameData(Asset, NewPath, AssetName));

	bool bSuccess = AssetTools.RenameAssets(RenameData);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), bSuccess);
	Data->SetStringField(TEXT("new_path"), NewPath / AssetName + TEXT(".") + AssetName);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params || !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'asset_path' required"));

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));

	// MCP에서는 항상 다이얼로그 없이 삭제 (GameThread 블록 방지)
	TArray<UObject*> AssetsToDelete;
	AssetsToDelete.Add(Asset);

	// ForceDeleteObjects는 참조 확인 다이얼로그를 건너뜀
	int32 Deleted = ObjectTools::ForceDeleteObjects(AssetsToDelete, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), Deleted > 0);
	Data->SetStringField(TEXT("deleted_asset"), AssetPath);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, NewName;
	if (!Params || !Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'asset_path' required"));
	if (!Params->TryGetStringField(TEXT("new_name"), NewName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'new_name' required"));

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));

	FString NewPath;
	if (!Params->TryGetStringField(TEXT("new_path"), NewPath))
		NewPath = FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.DuplicateAsset(NewName, NewPath, Asset);

	if (!NewAsset)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to duplicate asset"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("new_asset_path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("class"), NewAsset->GetClass()->GetName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPAssetHandler::HandleCreateAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName, Name;
	if (!Params || !Params->TryGetStringField(TEXT("class_name"), ClassName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'class_name' required"));
	if (!Params->TryGetStringField(TEXT("name"), Name))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'name' required"));

	// 1. UClass 해석 (접두어 U 없이도 동작)
	UClass* AssetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
	if (!AssetClass)
		AssetClass = FindFirstObject<UClass>(*(FString(TEXT("U")) + ClassName), EFindFirstObjectOptions::NativeFirst);
	if (!AssetClass)
		return ErrorResponse(TEXT("CLASS_NOT_FOUND"),
			FString::Printf(TEXT("UClass '%s' not found"), *ClassName));

	// 2. 패키지 경로
	FString Path = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), Path);
	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);

	// 3. Factory 자동 탐색 (특수 케이스 우선, 이후 자동 검색)
	UFactory* Factory = nullptr;
	if (AssetClass == UDataTable::StaticClass())
	{
		Factory = NewObject<UDataTableFactory>();
	}
	else
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UFactory::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
			{
				UFactory* CDO = Cast<UFactory>(It->GetDefaultObject());
				if (CDO && CDO->SupportedClass == AssetClass && CDO->CanCreateNew())
				{
					Factory = NewObject<UFactory>(GetTransientPackage(), *It);
					break;
				}
			}
		}
	}

	// 4. Factory 선행 설정 (특수 케이스)
	const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
	Params->TryGetObjectField(TEXT("properties"), PropertiesPtr);

	if (UDataTableFactory* DTFactory = Cast<UDataTableFactory>(Factory))
	{
		FString RowStructName;
		if (PropertiesPtr && (*PropertiesPtr)->TryGetStringField(TEXT("row_struct"), RowStructName))
		{
			UScriptStruct* RowStruct = FindFirstObject<UScriptStruct>(*RowStructName, EFindFirstObjectOptions::NativeFirst);
			if (!RowStruct)
			{
				FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *RowStructName);
				RowStruct = FindObject<UScriptStruct>(nullptr, *FullPath);
			}
			if (RowStruct)
				DTFactory->Struct = RowStruct;
			else
				return ErrorResponse(TEXT("INVALID_PARAMS"),
					FString::Printf(TEXT("Row struct '%s' not found"), *RowStructName));
		}
		else
		{
			return ErrorResponse(TEXT("INVALID_PARAMS"),
				TEXT("DataTable requires 'row_struct' in properties"));
		}
	}

	// 5. 에셋 생성
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: create_asset")));

	UObject* Asset = nullptr;
	if (Factory)
	{
		Asset = Factory->FactoryCreateNew(
			AssetClass, Package, FName(*Name),
			RF_Public | RF_Standalone, nullptr, GWarn);
	}
	else
	{
		Asset = NewObject<UObject>(Package, AssetClass, FName(*Name),
			RF_Public | RF_Standalone);
	}

	if (!Asset)
		return ErrorResponse(TEXT("CREATION_FAILED"),
			FString::Printf(TEXT("Failed to create asset of class '%s'"), *ClassName));

	// 6. 프로퍼티 설정
	TArray<FString> PropertyWarnings;
	if (PropertiesPtr)
	{
		for (const auto& Pair : (*PropertiesPtr)->Values)
		{
			if (Pair.Key == TEXT("row_struct")) continue;

			FProperty* Prop = AssetClass->FindPropertyByName(FName(*Pair.Key));
			if (!Prop)
			{
				PropertyWarnings.Add(FString::Printf(TEXT("Property '%s' not found on %s"), *Pair.Key, *ClassName));
				continue;
			}

			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Asset);

			if (Pair.Value->Type == EJson::String)
			{
				FString StrVal = Pair.Value->AsString();
				if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
					StrProp->SetPropertyValue(ValuePtr, StrVal);
				else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
					NameProp->SetPropertyValue(ValuePtr, FName(*StrVal));
				else if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
					TextProp->SetPropertyValue(ValuePtr, FText::FromString(StrVal));
				else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
				{
					UEnum* Enum = EnumProp->GetEnum();
					int64 EnumVal = Enum->GetValueByNameString(StrVal);
					if (EnumVal != INDEX_NONE)
						EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumVal);
				}
				else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
				{
					UObject* Ref = LoadObject<UObject>(nullptr, *StrVal);
					if (Ref) ObjProp->SetObjectPropertyValue(ValuePtr, Ref);
				}
				else
				{
					Prop->ImportText_Direct(*StrVal, ValuePtr, Asset, PPF_None);
				}
			}
			else if (Pair.Value->Type == EJson::Number)
			{
				double NumVal = Pair.Value->AsNumber();
				if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
					FloatProp->SetPropertyValue(ValuePtr, (float)NumVal);
				else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
					DoubleProp->SetPropertyValue(ValuePtr, NumVal);
				else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
					IntProp->SetPropertyValue(ValuePtr, (int32)NumVal);
			}
			else if (Pair.Value->Type == EJson::Boolean)
			{
				if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
					BoolProp->SetPropertyValue(ValuePtr, Pair.Value->AsBool());
			}
		}
	}

	// 7. 등록
	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();

	// 8. 응답
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), Asset->GetPathName());
	Data->SetStringField(TEXT("class"), AssetClass->GetName());
	Data->SetBoolField(TEXT("factory_used"), Factory != nullptr);

	if (PropertyWarnings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Warnings;
		for (const FString& W : PropertyWarnings)
			Warnings.Add(MakeShared<FJsonValueString>(W));
		Data->SetArrayField(TEXT("property_warnings"), Warnings);
	}

	return SuccessResponse(Data);
}
