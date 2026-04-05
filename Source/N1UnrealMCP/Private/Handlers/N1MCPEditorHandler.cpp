#include "Handlers/N1MCPEditorHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Light.h"
#include "Camera/CameraActor.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "FileHelpers.h"
#include "UnrealClient.h"

FN1MCPEditorHandler::FN1MCPEditorHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("editor"))
{
}

void FN1MCPEditorHandler::RegisterCommands()
{
	// 조회
	RegisterCommand(TEXT("get_actors_in_level"),
		TEXT("현재 레벨의 모든 액터 목록 반환"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetActorsInLevel(P); });

	RegisterCommand(TEXT("find_actors"),
		TEXT("액터 범용 검색 (이름/클래스/태그 필터)"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleFindActorsByName(P); });

	RegisterCommand(TEXT("get_actor_properties"),
		TEXT("액터 속성 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetActorProperties(P); });

	RegisterCommand(TEXT("get_level_info"),
		TEXT("현재 레벨 정보 반환"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetLevelInfo(P); });

	RegisterCommand(TEXT("get_selected_actors"),
		TEXT("에디터에서 선택된 액터 목록"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetSelectedActors(P); });

	// 변경형
	RegisterCommand(TEXT("set_actor_property"),
		TEXT("액터 속성 수정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetActorProperty(P); });

	RegisterCommand(TEXT("set_actor_transform"),
		TEXT("액터 위치/회전/스케일 변경"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetActorTransform(P); });

	RegisterCommand(TEXT("spawn_actor"),
		TEXT("네이티브 클래스로 액터 스폰"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSpawnActor(P); });

	RegisterCommand(TEXT("spawn_blueprint_actor"),
		TEXT("블루프린트 클래스로 액터 스폰"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSpawnBlueprintActor(P); });

	RegisterCommand(TEXT("duplicate_actor"),
		TEXT("액터 복제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDuplicateActor(P); });

	RegisterCommand(TEXT("delete_actor"),
		TEXT("액터 삭제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDeleteActor(P); });

	RegisterCommand(TEXT("rename_actor"),
		TEXT("액터 이름 변경"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleRenameActor(P); });

	RegisterCommand(TEXT("set_actor_mobility"),
		TEXT("액터 모빌리티 설정 (Static/Stationary/Movable)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetActorMobility(P); });

	RegisterCommand(TEXT("attach_actor"),
		TEXT("액터를 다른 액터에 어태치"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAttachActor(P); });

	RegisterCommand(TEXT("detach_actor"),
		TEXT("액터 어태치 해제"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDetachActor(P); });

	RegisterCommand(TEXT("set_actor_tags"),
		TEXT("액터 태그 설정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetActorTags(P); });

	RegisterCommand(TEXT("set_component_material"),
		TEXT("액터 컴포넌트의 머티리얼 슬롯 설정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetComponentMaterial(P); });

	RegisterCommand(TEXT("focus_viewport"),
		TEXT("뷰포트 카메라를 액터에 포커스"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleFocusViewport(P); });

	RegisterCommand(TEXT("take_screenshot"),
		TEXT("뷰포트 스크린샷 캡처"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleTakeScreenshot(P); });

	RegisterCommand(TEXT("open_level"),
		TEXT("레벨(맵) 열기"), nullptr,
		false, false, false, 30000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleOpenLevel(P); });
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::ActorToJson(AActor* Actor, bool bDetailed)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("display_name"), Actor->GetActorLabel());
	Obj->SetStringField(TEXT("object_path"), Actor->GetPathName());
	Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	if (bDetailed)
	{
		FVector Loc = Actor->GetActorLocation();
		FRotator Rot = Actor->GetActorRotation();
		FVector Scale = Actor->GetActorScale3D();

		TArray<TSharedPtr<FJsonValue>> LocArr;
		LocArr.Add(MakeShared<FJsonValueNumber>(Loc.X));
		LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Y));
		LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Z));
		Obj->SetArrayField(TEXT("location"), LocArr);

		TArray<TSharedPtr<FJsonValue>> RotArr;
		RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Pitch));
		RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Yaw));
		RotArr.Add(MakeShared<FJsonValueNumber>(Rot.Roll));
		Obj->SetArrayField(TEXT("rotation"), RotArr);

		TArray<TSharedPtr<FJsonValue>> ScaleArr;
		ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.X));
		ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Y));
		ScaleArr.Add(MakeShared<FJsonValueNumber>(Scale.Z));
		Obj->SetArrayField(TEXT("scale"), ScaleArr);
	}

	return Obj;
}

// ── 조회 커맨드 ──

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor world"));

	FString ClassFilter, TagFilter;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
		Params->TryGetStringField(TEXT("tag_filter"), TagFilter);
	}

	// detail 옵션: "full"이면 transform 포함, 기본은 summary (이름+클래스만)
	FString Detail;
	bool bDetailed = false;
	if (Params.IsValid() && Params->TryGetStringField(TEXT("detail"), Detail))
		bDetailed = (Detail == TEXT("full"));

	TArray<TSharedPtr<FJsonValue>> AllActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!ClassFilter.IsEmpty() && !Actor->GetClass()->GetName().Contains(ClassFilter))
			continue;
		if (!TagFilter.IsEmpty() && !Actor->Tags.ContainsByPredicate(
			[&](const FName& Tag) { return Tag.ToString() == TagFilter; }))
			continue;
		AllActors.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor, bDetailed)));
	}

	TSharedPtr<FJsonObject> Data = FN1MCPCommandRegistry::ApplyPagination(AllActors, Params);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor world"));

	FString NamePattern, ClassFilter, TagFilter;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("pattern"), NamePattern);
		Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
		Params->TryGetStringField(TEXT("tag_filter"), TagFilter);
	}

	// 최소 하나의 필터는 있어야 함
	if (NamePattern.IsEmpty() && ClassFilter.IsEmpty() && TagFilter.IsEmpty())
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			TEXT("At least one filter required: 'pattern', 'class_filter', or 'tag_filter'"));

	FString Detail;
	bool bDetailed = false;
	if (Params.IsValid() && Params->TryGetStringField(TEXT("detail"), Detail))
		bDetailed = (Detail == TEXT("full"));

	TArray<TSharedPtr<FJsonValue>> AllActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		if (!NamePattern.IsEmpty())
		{
			// 와일드카드(*) 지원: *를 제거하고 Contains 매칭
			FString CleanPattern = NamePattern.Replace(TEXT("*"), TEXT(""));
			if (!Actor->GetActorLabel().Contains(CleanPattern) &&
				!Actor->GetName().Contains(CleanPattern))
				continue;
		}

		if (!ClassFilter.IsEmpty() &&
			!Actor->GetClass()->GetName().Contains(ClassFilter))
			continue;

		if (!TagFilter.IsEmpty() &&
			!Actor->Tags.ContainsByPredicate(
				[&](const FName& Tag) { return Tag.ToString().Contains(TagFilter); }))
			continue;

		AllActors.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor, bDetailed)));
	}

	TSharedPtr<FJsonObject> Data = FN1MCPCommandRegistry::ApplyPagination(AllActors, Params);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));

	TArray<FString> MatchedPaths;
	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error, &MatchedPaths);
	if (!Actor)
	{
		if (MatchedPaths.Num() > 0)
		{
			TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& P : MatchedPaths) Arr.Add(MakeShared<FJsonValueString>(P));
			Extra->SetArrayField(TEXT("matched_actors"), Arr);
			return ErrorResponseWithData(TEXT("AMBIGUOUS_NAME"), Error, Extra);
		}
		return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);
	}

	TSharedPtr<FJsonObject> Data = ActorToJson(Actor);

	// mobility
	USceneComponent* Root = Actor->GetRootComponent();
	if (Root)
	{
		FString Mobility;
		switch (Root->Mobility)
		{
		case EComponentMobility::Static: Mobility = TEXT("Static"); break;
		case EComponentMobility::Stationary: Mobility = TEXT("Stationary"); break;
		case EComponentMobility::Movable: Mobility = TEXT("Movable"); break;
		}
		Data->SetStringField(TEXT("mobility"), Mobility);
	}

	// tags
	TArray<TSharedPtr<FJsonValue>> TagArr;
	for (const FName& Tag : Actor->Tags)
		TagArr.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	Data->SetArrayField(TEXT("tags"), TagArr);

	// parent
	if (Actor->GetAttachParentActor())
		Data->SetStringField(TEXT("parent"), Actor->GetAttachParentActor()->GetPathName());

	// components (C++ CreateDefaultSubobject + BP 추가 컴포넌트 모두 포함)
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	TArray<TSharedPtr<FJsonValue>> CompArr;
	for (UActorComponent* Comp : Components)
	{
		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Comp->GetName());
		CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

		if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
		{
			CompObj->SetStringField(TEXT("static_mesh"),
				SMC->GetStaticMesh() ? SMC->GetStaticMesh()->GetPathName() : TEXT("None"));
		}

		if (USceneComponent* SC = Cast<USceneComponent>(Comp))
		{
			FVector RelLoc = SC->GetRelativeLocation();
			TArray<TSharedPtr<FJsonValue>> LocArr;
			LocArr.Add(MakeShared<FJsonValueNumber>(RelLoc.X));
			LocArr.Add(MakeShared<FJsonValueNumber>(RelLoc.Y));
			LocArr.Add(MakeShared<FJsonValueNumber>(RelLoc.Z));
			CompObj->SetArrayField(TEXT("relative_location"), LocArr);
		}

		CompArr.Add(MakeShared<FJsonValueObject>(CompObj));
	}
	Data->SetArrayField(TEXT("components"), CompArr);

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleGetLevelInfo(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor world"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("level_name"), World->GetMapName());

	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It) ActorCount++;
	Data->SetNumberField(TEXT("actor_count"), ActorCount);

	TArray<TSharedPtr<FJsonValue>> SubLevels;
	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (Streaming)
			SubLevels.Add(MakeShared<FJsonValueString>(Streaming->GetWorldAssetPackageName()));
	}
	Data->SetArrayField(TEXT("sub_levels"), SubLevels);

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> Selected;
	USelection* Selection = GEditor->GetSelectedActors();
	for (int32 i = 0; i < Selection->Num(); ++i)
	{
		AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
		if (Actor)
			Selected.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), Selected);
	Data->SetNumberField(TEXT("count"), Selected.Num());
	return SuccessResponse(Data);
}

// ── 변경형 커맨드 ──

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef, PropName;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));
	if (!Params->TryGetStringField(TEXT("property"), PropName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'property' required"));
	if (!Params->HasField(TEXT("value")))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'value' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_actor_property")));
	FString MutError;
	if (!BeginMutation(Actor, Params, MutError))
		return ErrorResponse(TEXT("PERMISSION_DENIED"), MutError);

	// 점 표기법 지원: "ComponentName.PropertyName"
	UObject* Target = Actor;
	FString ResolvedPropName = PropName;

	if (PropName.Contains(TEXT(".")))
	{
		FString CompName;
		PropName.Split(TEXT("."), &CompName, &ResolvedPropName);

		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		UActorComponent* FoundComp = nullptr;
		for (UActorComponent* Comp : Components)
		{
			if (Comp->GetName() == CompName)
			{
				FoundComp = Comp;
				break;
			}
		}
		if (!FoundComp)
			return ErrorResponse(TEXT("COMPONENT_NOT_FOUND"),
				FString::Printf(TEXT("Component '%s' not found on '%s'"), *CompName, *Actor->GetActorLabel()));

		FoundComp->Modify();
		Target = FoundComp;
	}

	FProperty* Prop = Target->GetClass()->FindPropertyByName(FName(*ResolvedPropName));
	if (!Prop)
	{
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Property '%s' not found on '%s'"), *ResolvedPropName, *Target->GetClass()->GetName()));
	}

	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Target);

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
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		NameProp->SetPropertyValue(PropAddr, FName(*Value->AsString()));
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

	EndMutation(Actor, Params, MutError);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor"), Actor->GetPathName());
	Data->SetStringField(TEXT("property"), PropName);
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_actor_transform")));
	FString MutError;
	if (!BeginMutation(Actor, Params, MutError))
		return ErrorResponse(TEXT("PERMISSION_DENIED"), MutError);

	if (Params->HasField(TEXT("location")))
		Actor->SetActorLocation(GetVectorFromJson(Params, TEXT("location")));
	if (Params->HasField(TEXT("rotation")))
		Actor->SetActorRotation(GetRotatorFromJson(Params, TEXT("rotation")));
	if (Params->HasField(TEXT("scale")))
		Actor->SetActorScale3D(GetVectorFromJson(Params, TEXT("scale")));

	EndMutation(Actor, Params, MutError);
	return SuccessResponse(ActorToJson(Actor));
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassPath;
	if (!Params || !Params->TryGetStringField(TEXT("class_path"), ClassPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'class_path' required"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor world"));

	UClass* ActorClass = FindObject<UClass>(nullptr, *ClassPath);
	if (!ActorClass)
		ActorClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (!ActorClass)
	{
		// 짧은 이름으로 시도 (PointLight, StaticMeshActor 등)
		FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassPath);
		ActorClass = LoadClass<AActor>(nullptr, *FullPath);
	}
	if (!ActorClass)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Class '%s' not found"), *ClassPath));

	FVector Location = GetVectorFromJson(Params, TEXT("location"));
	FRotator Rotation = GetRotatorFromJson(Params, TEXT("rotation"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: spawn_actor")));

	FActorSpawnParameters SpawnParams;
	FString Name;
	if (Params->TryGetStringField(TEXT("name"), Name))
	{
		SpawnParams.Name = FName(*Name);
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (!NewActor)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to spawn actor"));

	if (Params->HasField(TEXT("scale")))
		NewActor->SetActorScale3D(GetVectorFromJson(Params, TEXT("scale")));

	// StaticMeshActor인 경우 static_mesh 파라미터로 메쉬 설정
	FString MeshPath;
	if (Params->TryGetStringField(TEXT("static_mesh"), MeshPath))
	{
		if (AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(NewActor))
		{
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
			if (Mesh && SMActor->GetStaticMeshComponent())
			{
				SMActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			}
		}
	}

	return SuccessResponse(ActorToJson(NewActor));
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
	FString BPPath;
	if (!Params || !Params->TryGetStringField(TEXT("blueprint_path"), BPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'blueprint_path' required"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor world"));

	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BPPath);
	if (!BP)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Blueprint '%s' not found"), *BPPath));

	UClass* BPClass = BP->GeneratedClass;
	if (!BPClass)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Blueprint has no generated class"));

	FVector Location = GetVectorFromJson(Params, TEXT("location"));
	FRotator Rotation = GetRotatorFromJson(Params, TEXT("rotation"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: spawn_blueprint_actor")));

	FActorSpawnParameters SpawnParams;
	FString Name;
	if (Params->TryGetStringField(TEXT("name"), Name))
	{
		SpawnParams.Name = FName(*Name);
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(BPClass, Location, Rotation, SpawnParams);
	if (!NewActor)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to spawn blueprint actor"));

	if (Params->HasField(TEXT("scale")))
		NewActor->SetActorScale3D(GetVectorFromJson(Params, TEXT("scale")));

	return SuccessResponse(ActorToJson(NewActor));
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	UWorld* World = Actor->GetWorld();
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: duplicate_actor")));

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Actor, true, false);
	GEditor->edactDuplicateSelected(World->GetCurrentLevel(), false);

	AActor* NewActor = nullptr;
	USelection* Selection = GEditor->GetSelectedActors();
	if (Selection->Num() > 0)
		NewActor = Cast<AActor>(Selection->GetSelectedObject(0));

	if (!NewActor)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to duplicate actor"));

	if (Params->HasField(TEXT("offset")))
	{
		FVector Offset = GetVectorFromJson(Params, TEXT("offset"));
		NewActor->SetActorLocation(NewActor->GetActorLocation() + Offset);
	}

	FString NewName;
	if (Params->TryGetStringField(TEXT("new_name"), NewName))
		NewActor->SetActorLabel(NewName);

	return SuccessResponse(ActorToJson(NewActor));
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	FString Path = Actor->GetPathName();
	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: delete_actor")));
	bool bDestroyed = GEditor->GetEditorWorldContext().World()->DestroyActor(Actor);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), bDestroyed);
	Data->SetStringField(TEXT("deleted_actor"), Path);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleRenameActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef, NewName;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));
	if (!Params->TryGetStringField(TEXT("new_name"), NewName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'new_name' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: rename_actor")));
	Actor->SetActorLabel(NewName);

	return SuccessResponse(ActorToJson(Actor));
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleSetActorMobility(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef, Mobility;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));
	if (!Params->TryGetStringField(TEXT("mobility"), Mobility))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'mobility' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	USceneComponent* Root = Actor->GetRootComponent();
	if (!Root) return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Actor has no root component"));

	EComponentMobility::Type MobilityType;
	if (Mobility == TEXT("Static")) MobilityType = EComponentMobility::Static;
	else if (Mobility == TEXT("Stationary")) MobilityType = EComponentMobility::Stationary;
	else if (Mobility == TEXT("Movable")) MobilityType = EComponentMobility::Movable;
	else return ErrorResponse(TEXT("INVALID_PARAMS"),
		FString::Printf(TEXT("Invalid mobility: '%s' (use Static/Stationary/Movable)"), *Mobility));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_actor_mobility")));
	FString MutError;
	if (!BeginMutation(Root, Params, MutError))
		return ErrorResponse(TEXT("PERMISSION_DENIED"), MutError);

	Root->SetMobility(MobilityType);
	EndMutation(Root, Params, MutError);

	return SuccessResponse(ActorToJson(Actor));
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleAttachActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef, ParentRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'parent_name' required"));

	FString Error;
	AActor* Child = FindActorByRef(ActorRef, Error);
	if (!Child) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);
	AActor* Parent = FindActorByRef(ParentRef, Error);
	if (!Parent) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: attach_actor")));

	FName SocketName = NAME_None;
	FString Socket;
	if (Params->TryGetStringField(TEXT("socket"), Socket))
		SocketName = FName(*Socket);

	Child->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform, SocketName);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("child"), Child->GetPathName());
	Data->SetStringField(TEXT("parent"), Parent->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleDetachActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: detach_actor")));
	Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("actor"), Actor->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleSetActorTags(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));

	const TArray<TSharedPtr<FJsonValue>>* TagsArr;
	if (!Params->TryGetArrayField(TEXT("tags"), TagsArr))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'tags' array required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_actor_tags")));
	FString MutError;
	if (!BeginMutation(Actor, Params, MutError))
		return ErrorResponse(TEXT("PERMISSION_DENIED"), MutError);

	Actor->Tags.Empty();
	for (const auto& Val : *TagsArr)
		Actor->Tags.Add(FName(*Val->AsString()));

	EndMutation(Actor, Params, MutError);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("actor"), Actor->GetPathName());
	Data->SetNumberField(TEXT("tag_count"), Actor->Tags.Num());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleSetComponentMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef, CompName, MatPath;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));
	if (!Params->TryGetStringField(TEXT("component_name"), CompName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'component_name' required"));
	if (!Params->TryGetStringField(TEXT("material_path"), MatPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'material_path' required"));

	int32 SlotIndex = 0;
	if (Params->HasField(TEXT("slot_index")))
		SlotIndex = static_cast<int32>(Params->GetNumberField(TEXT("slot_index")));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	UPrimitiveComponent* PrimComp = nullptr;
	for (UActorComponent* Comp : Components)
	{
		if (Comp->GetName() == CompName)
		{
			PrimComp = Cast<UPrimitiveComponent>(Comp);
			break;
		}
	}
	if (!PrimComp)
		return ErrorResponse(TEXT("COMPONENT_NOT_FOUND"),
			FString::Printf(TEXT("PrimitiveComponent '%s' not found"), *CompName));

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MatPath);
	if (!Mat)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Material '%s' not found"), *MatPath));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_component_material")));
	PrimComp->Modify();
	PrimComp->SetMaterial(SlotIndex, Mat);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("component"), CompName);
	Data->SetStringField(TEXT("material"), MatPath);
	Data->SetNumberField(TEXT("slot_index"), SlotIndex);
	return SuccessResponse(Data);
}

// ── 에디터 유틸 ──

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorRef;
	if (!Params || !Params->TryGetStringField(TEXT("actor_name"), ActorRef))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'actor_name' required"));

	FString Error;
	AActor* Actor = FindActorByRef(ActorRef, Error);
	if (!Actor) return ErrorResponse(TEXT("ACTOR_NOT_FOUND"), Error);

	GEditor->MoveViewportCamerasToActor(*Actor, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("focused_on"), Actor->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
	FString Filename = FPaths::ProjectSavedDir() / TEXT("Screenshots/N1MCP_Screenshot.png");
	if (Params.IsValid())
		Params->TryGetStringField(TEXT("filename"), Filename);

	FViewport* Viewport = GEditor->GetActiveViewport();
	if (!Viewport)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No active viewport"));

	TArray<FColor> Bitmap;
	int32 Width = Viewport->GetSizeXY().X;
	int32 Height = Viewport->GetSizeXY().Y;

	if (!Viewport->ReadPixels(Bitmap) || Bitmap.Num() == 0)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to read viewport pixels"));

	TArray64<uint8> PngData;
	FImageUtils::PNGCompressImageArray(Width, Height, Bitmap, PngData);
	FFileHelper::SaveArrayToFile(PngData, *Filename);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("file_path"), Filename);
	Data->SetNumberField(TEXT("width"), Width);
	Data->SetNumberField(TEXT("height"), Height);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPEditorHandler::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
	FString LevelPath;
	if (!Params || !Params->TryGetStringField(TEXT("level_path"), LevelPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'level_path' required"));

	// 짧은 경로 지원: EditorStartupLevel → /Game/Level/EditorStartupLevel
	if (!LevelPath.StartsWith(TEXT("/")))
	{
		LevelPath = TEXT("/Game/Level/") + LevelPath;
	}

	// .umap 확장자 제거
	LevelPath.RemoveFromEnd(TEXT(".umap"));

	FString MapFilePath;
	if (!FPackageName::DoesPackageExist(LevelPath, &MapFilePath))
	{
		// /Game/Maps/ 경로도 시도
		FString AltPath = LevelPath.Replace(TEXT("/Game/Level/"), TEXT("/Game/Maps/"));
		if (FPackageName::DoesPackageExist(AltPath, &MapFilePath))
		{
			LevelPath = AltPath;
		}
		else
		{
			return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
				FString::Printf(TEXT("Level not found: '%s'"), *LevelPath));
		}
	}

	FEditorFileUtils::LoadMap(LevelPath);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("level"), LevelPath);
	return SuccessResponse(Data);
}
