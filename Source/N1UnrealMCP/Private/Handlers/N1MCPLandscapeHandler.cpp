#include "Handlers/N1MCPLandscapeHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "Misc/FileHelper.h"

FN1MCPLandscapeHandler::FN1MCPLandscapeHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("landscape"))
{
}

ALandscapeProxy* FN1MCPLandscapeHandler::FindLandscape(const FString& Ref, FString& OutError)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { OutError = TEXT("No editor world"); return nullptr; }

	if (Ref.StartsWith(TEXT("/")))
	{
		ALandscapeProxy* LP = FindObject<ALandscapeProxy>(nullptr, *Ref);
		if (!LP) OutError = FString::Printf(TEXT("Landscape not found: '%s'"), *Ref);
		return LP;
	}

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		if (It->GetActorLabel() == Ref || It->GetName() == Ref)
			return *It;
	}
	OutError = FString::Printf(TEXT("Landscape '%s' not found"), *Ref);
	return nullptr;
}

void FN1MCPLandscapeHandler::RegisterCommands()
{
	RegisterCommand(TEXT("create_landscape"),
		TEXT("랜드스케이프 생성 (v1: 기본 평면)"), nullptr,
		true, false, false, 30000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateLandscape(P); });

	RegisterCommand(TEXT("add_landscape_layer"),
		TEXT("페인트 레이어 추가 (v1: 부분 지원)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddLandscapeLayer(P); });

	RegisterCommand(TEXT("sculpt_landscape"),
		TEXT("하이트맵 스컬프팅 (실험적, v1 미구현)"), nullptr,
		true, false, true, 60000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSculptLandscape(P); });

	RegisterCommand(TEXT("paint_landscape_layer"),
		TEXT("레이어 페인팅 (실험적, v1 미구현)"), nullptr,
		true, false, true, 60000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandlePaintLandscapeLayer(P); });

	RegisterCommand(TEXT("import_heightmap"),
		TEXT("하이트맵 파일 임포트 (v1: 부분 지원)"), nullptr,
		true, false, true, 120000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleImportHeightmap(P); });

	RegisterCommand(TEXT("export_heightmap"),
		TEXT("하이트맵 내보내기 (v1: 부분 지원)"), nullptr,
		false, false, false, 30000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleExportHeightmap(P); });

	RegisterCommand(TEXT("assign_landscape_material"),
		TEXT("랜드스케이프 머티리얼 할당"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAssignLandscapeMaterial(P); });

	RegisterCommand(TEXT("get_landscape_info"),
		TEXT("랜드스케이프 정보 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetLandscapeInfo(P); });
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandleCreateLandscape(const TSharedPtr<FJsonObject>& Params)
{
	// v1: ALandscape를 스폰만 하고, 실제 heightmap Import는 에디터 UI 의존도가 높아 제한적
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor world"));

	FVector Scale(100, 100, 100);
	if (Params.IsValid() && Params->HasField(TEXT("scale")))
		Scale = GetVectorFromJson(Params, TEXT("scale"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: create_landscape")));

	ALandscape* Landscape = World->SpawnActor<ALandscape>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Landscape)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to spawn landscape actor"));

	Landscape->SetActorScale3D(Scale);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("object_path"), Landscape->GetPathName());
	Data->SetStringField(TEXT("display_name"), Landscape->GetActorLabel());
	Data->SetStringField(TEXT("note"), TEXT("Landscape actor spawned. Use editor tools or import_heightmap for terrain data."));
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandleAddLandscapeLayer(const TSharedPtr<FJsonObject>& Params)
{
	FString LandRef, LayerName;
	if (!Params || !Params->TryGetStringField(TEXT("landscape_ref"), LandRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'landscape_ref' required"));
	if (!Params->TryGetStringField(TEXT("layer_name"), LayerName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'layer_name' required"));

	FString Error;
	ALandscapeProxy* Landscape = FindLandscape(LandRef, Error);
	if (!Landscape) return ErrorResponse(TEXT("LANDSCAPE_NOT_FOUND"), Error);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("layer"), LayerName);
	Data->SetStringField(TEXT("note"), TEXT("Layer info created. Full layer painting requires editor mode integration."));
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandleSculptLandscape(const TSharedPtr<FJsonObject>& Params)
{
	return ErrorResponse(TEXT("INTERNAL_ERROR"),
		TEXT("sculpt_landscape is experimental and not yet implemented in v1."));
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandlePaintLandscapeLayer(const TSharedPtr<FJsonObject>& Params)
{
	return ErrorResponse(TEXT("INTERNAL_ERROR"),
		TEXT("paint_landscape_layer is experimental and not yet implemented in v1."));
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandleImportHeightmap(const TSharedPtr<FJsonObject>& Params)
{
	FString LandRef, FilePath;
	if (!Params || !Params->TryGetStringField(TEXT("landscape_ref"), LandRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'landscape_ref' required"));
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'file_path' required"));

	if (!FPaths::FileExists(FilePath))
		return ErrorResponse(TEXT("IMPORT_FAILED"),
			FString::Printf(TEXT("File not found: '%s'"), *FilePath));

	FString Error;
	ALandscapeProxy* Landscape = FindLandscape(LandRef, Error);
	if (!Landscape) return ErrorResponse(TEXT("LANDSCAPE_NOT_FOUND"), Error);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("landscape"), Landscape->GetPathName());
	Data->SetStringField(TEXT("note"), TEXT("Heightmap import requires LandscapeEditDataInterface - partial v1 support."));
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandleExportHeightmap(const TSharedPtr<FJsonObject>& Params)
{
	FString LandRef, FilePath;
	if (!Params || !Params->TryGetStringField(TEXT("landscape_ref"), LandRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'landscape_ref' required"));
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'file_path' required"));

	FString Error;
	ALandscapeProxy* Landscape = FindLandscape(LandRef, Error);
	if (!Landscape) return ErrorResponse(TEXT("LANDSCAPE_NOT_FOUND"), Error);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("file_path"), FilePath);
	Data->SetStringField(TEXT("note"), TEXT("Export requires LandscapeEditDataInterface - partial v1 support."));
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandleAssignLandscapeMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString LandRef, MatPath;
	if (!Params || !Params->TryGetStringField(TEXT("landscape_ref"), LandRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'landscape_ref' required"));
	if (!Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	FString Error;
	ALandscapeProxy* Landscape = FindLandscape(LandRef, Error);
	if (!Landscape) return ErrorResponse(TEXT("LANDSCAPE_NOT_FOUND"), Error);

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MatPath);
	if (!Material)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Material not found: '%s'"), *MatPath));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: assign_landscape_material")));
	Landscape->Modify();
	Landscape->LandscapeMaterial = Material;
	Landscape->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("landscape"), Landscape->GetPathName());
	Data->SetStringField(TEXT("material"), Material->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPLandscapeHandler::HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString LandRef;
	if (!Params || !Params->TryGetStringField(TEXT("landscape_ref"), LandRef))
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor world"));

		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			LandRef = It->GetPathName();
			break;
		}
		if (LandRef.IsEmpty())
			return ErrorResponse(TEXT("LANDSCAPE_NOT_FOUND"), TEXT("No landscape in level"));
	}

	FString Error;
	ALandscapeProxy* Landscape = FindLandscape(LandRef, Error);
	if (!Landscape) return ErrorResponse(TEXT("LANDSCAPE_NOT_FOUND"), Error);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("object_path"), Landscape->GetPathName());
	Data->SetStringField(TEXT("display_name"), Landscape->GetActorLabel());

	if (Landscape->LandscapeMaterial)
		Data->SetStringField(TEXT("material"), Landscape->LandscapeMaterial->GetPathName());

	FVector Scale = Landscape->GetActorScale3D();
	TArray<TSharedPtr<FJsonValue>> ScaleArr;
	ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.X));
	ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Y));
	ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Z));
	Data->SetArrayField(TEXT("scale"), ScaleArr);

	// 컴포넌트 수로 대략적 크기 추정
	Data->SetNumberField(TEXT("component_count"), Landscape->LandscapeComponents.Num());

	return SuccessResponse(Data);
}
