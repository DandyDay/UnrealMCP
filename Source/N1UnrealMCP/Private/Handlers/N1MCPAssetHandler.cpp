#include "Handlers/N1MCPAssetHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"

FN1MCPAssetHandler::FN1MCPAssetHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("asset"))
{
}

void FN1MCPAssetHandler::RegisterCommands()
{
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

	TArray<FString> Files;
	Files.Add(SourceFile);

	TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(Files, DestPath);

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

	bool bForce = false;
	if (Params->HasField(TEXT("force")))
		bForce = Params->GetBoolField(TEXT("force"));

	TArray<UObject*> AssetsToDelete;
	AssetsToDelete.Add(Asset);

	int32 Deleted = ObjectTools::DeleteObjects(AssetsToDelete, !bForce);

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
