#include "N1MCPHandlerBase.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"

FN1MCPHandlerBase::FN1MCPHandlerBase(FN1MCPCommandRegistry& InRegistry, const FString& InCategory)
	: Registry(InRegistry), Category(InCategory)
{
}

void FN1MCPHandlerBase::RegisterCommand(
	const FString& Name, const FString& Description,
	const TSharedPtr<FJsonObject>& ParameterSchema,
	bool bMutatesEditorState, bool bRequiresPIE, bool bLongRunning,
	int32 DefaultTimeoutMs,
	TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Func)
{
	FN1MCPCommandEntry Entry;
	Entry.CommandName = Name;
	Entry.Category = Category;
	Entry.Description = Description;
	Entry.ParameterSchema = ParameterSchema;
	Entry.bMutatesEditorState = bMutatesEditorState;
	Entry.bRequiresPIE = bRequiresPIE;
	Entry.bLongRunning = bLongRunning;
	Entry.DefaultTimeoutMs = DefaultTimeoutMs;
	Entry.Handler = MoveTemp(Func);
	Registry.Register(Entry);
}

void FN1MCPHandlerBase::RegisterCommand(
	const FString& Name, const FString& Description,
	const TSharedPtr<FJsonObject>& ParameterSchema,
	TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Func)
{
	RegisterCommand(Name, Description, ParameterSchema,
		false, false, false, 10000, MoveTemp(Func));
}

TSharedPtr<FJsonObject> FN1MCPHandlerBase::SuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("status"), TEXT("success"));
	Response->SetObjectField(TEXT("result"), Data);
	return Response;
}

TSharedPtr<FJsonObject> FN1MCPHandlerBase::ErrorResponse(
	const FString& ErrorCode, const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("status"), TEXT("error"));
	Response->SetStringField(TEXT("error_code"), ErrorCode);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FN1MCPHandlerBase::ErrorResponseWithData(
	const FString& ErrorCode, const FString& Message,
	const TSharedPtr<FJsonObject>& ExtraData)
{
	TSharedPtr<FJsonObject> Response = ErrorResponse(ErrorCode, Message);
	if (ExtraData.IsValid())
	{
		for (const auto& Pair : ExtraData->Values)
		{
			Response->SetField(Pair.Key, Pair.Value);
		}
	}
	return Response;
}

bool FN1MCPHandlerBase::BeginMutation(
	UObject* TargetObject,
	const TSharedPtr<FJsonObject>& Params,
	FString& OutError)
{
	if (!TargetObject)
	{
		OutError = TEXT("Target object is null");
		return false;
	}

	bool bCheckoutIfNeeded = true;
	if (Params.IsValid() && Params->HasField(TEXT("checkout_if_needed")))
	{
		bCheckoutIfNeeded = Params->GetBoolField(TEXT("checkout_if_needed"));
	}

	if (bCheckoutIfNeeded)
	{
		ISourceControlProvider& SCC = ISourceControlModule::Get().GetProvider();
		if (SCC.IsEnabled())
		{
			UPackage* Package = TargetObject->GetOutermost();
			FString PackageFilename;
			if (FPackageName::DoesPackageExist(Package->GetName(), &PackageFilename))
			{
				FSourceControlStatePtr State = SCC.GetState(
					PackageFilename, EStateCacheUsage::Use);
				if (State.IsValid() && !State->IsCheckedOut() && !State->IsAdded())
				{
					if (SCC.Execute(
						ISourceControlOperation::Create<FCheckOut>(),
						PackageFilename) != ECommandResult::Succeeded)
					{
						OutError = FString::Printf(
							TEXT("P4 checkout failed for '%s'"), *PackageFilename);
						return false;
					}
				}
			}
		}
	}

	TargetObject->Modify();
	return true;
}

bool FN1MCPHandlerBase::EndMutation(
	UObject* TargetObject,
	const TSharedPtr<FJsonObject>& Params,
	FString& OutError)
{
	if (!TargetObject)
	{
		OutError = TEXT("Target object is null");
		return false;
	}

	TargetObject->GetOutermost()->MarkPackageDirty();

	bool bSave = false;
	if (Params.IsValid() && Params->HasField(TEXT("save")))
	{
		bSave = Params->GetBoolField(TEXT("save"));
	}

	if (bSave)
	{
		UPackage* Package = TargetObject->GetOutermost();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(
			Package->GetName(), PackageFilename,
			Package->ContainsMap()
				? FPackageName::GetMapPackageExtension()
				: FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, nullptr, *PackageFilename, SaveArgs);
		}
	}

	return true;
}

FVector FN1MCPHandlerBase::GetVectorFromJson(
	const TSharedPtr<FJsonObject>& Obj, const FString& Field)
{
	FVector Result = FVector::ZeroVector;
	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr) && Arr->Num() >= 3)
	{
		Result.X = (*Arr)[0]->AsNumber();
		Result.Y = (*Arr)[1]->AsNumber();
		Result.Z = (*Arr)[2]->AsNumber();
	}
	return Result;
}

FRotator FN1MCPHandlerBase::GetRotatorFromJson(
	const TSharedPtr<FJsonObject>& Obj, const FString& Field)
{
	FRotator Result = FRotator::ZeroRotator;
	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr) && Arr->Num() >= 3)
	{
		Result.Pitch = (*Arr)[0]->AsNumber();
		Result.Yaw = (*Arr)[1]->AsNumber();
		Result.Roll = (*Arr)[2]->AsNumber();
	}
	return Result;
}

AActor* FN1MCPHandlerBase::FindActorByRef(
	const FString& ActorRef, FString& OutError, TArray<FString>* OutMatchedPaths)
{
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		OutError = TEXT("No editor world available");
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();

	// object_path로 직접 찾기
	if (ActorRef.StartsWith(TEXT("/")))
	{
		AActor* Actor = FindObject<AActor>(nullptr, *ActorRef);
		if (!Actor)
		{
			OutError = FString::Printf(
				TEXT("Actor not found at path: %s"), *ActorRef);
		}
		return Actor;
	}

	// 이름으로 검색 (AMBIGUOUS_NAME 처리)
	TArray<AActor*> Matches;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorRef || It->GetName() == ActorRef)
		{
			Matches.Add(*It);
		}
	}

	if (Matches.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Actor '%s' not found"), *ActorRef);
		return nullptr;
	}
	if (Matches.Num() > 1)
	{
		OutError = FString::Printf(
			TEXT("'%s' matches %d actors - use object_path for exact match"),
			*ActorRef, Matches.Num());
		if (OutMatchedPaths)
		{
			for (AActor* Match : Matches)
			{
				OutMatchedPaths->Add(Match->GetPathName());
			}
		}
		return nullptr;
	}
	return Matches[0];
}
