#pragma once

#include "CoreMinimal.h"
#include "N1MCPCommandRegistry.h"

class FN1MCPHandlerBase
{
public:
	FN1MCPHandlerBase(FN1MCPCommandRegistry& InRegistry, const FString& InCategory);
	virtual ~FN1MCPHandlerBase() = default;
	virtual void RegisterCommands() = 0;

protected:
	FN1MCPCommandRegistry& Registry;
	FString Category;

	// 변경형 커맨드 등록 (모든 플래그 지정)
	void RegisterCommand(
		const FString& Name, const FString& Description,
		const TSharedPtr<FJsonObject>& ParameterSchema,
		bool bMutatesEditorState, bool bRequiresPIE, bool bLongRunning,
		int32 DefaultTimeoutMs,
		TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Func
	);

	// 조회형 커맨드 등록 (플래그 기본값)
	void RegisterCommand(
		const FString& Name, const FString& Description,
		const TSharedPtr<FJsonObject>& ParameterSchema,
		TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Func
	);

	// 응답 헬퍼
	static TSharedPtr<FJsonObject> SuccessResponse(const TSharedPtr<FJsonObject>& Data);
	static TSharedPtr<FJsonObject> ErrorResponse(const FString& ErrorCode, const FString& Message);
	static TSharedPtr<FJsonObject> ErrorResponseWithData(
		const FString& ErrorCode, const FString& Message,
		const TSharedPtr<FJsonObject>& ExtraData
	);

	// Dirty/Save/P4 공통 헬퍼
	bool BeginMutation(UObject* TargetObject, const TSharedPtr<FJsonObject>& Params, FString& OutError);
	bool EndMutation(UObject* TargetObject, const TSharedPtr<FJsonObject>& Params, FString& OutError);

	// JSON 유틸
	static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& Obj, const FString& Field);
	static FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& Obj, const FString& Field);

	// 액터 검색 (object_path 또는 이름, AMBIGUOUS_NAME 처리)
	AActor* FindActorByRef(const FString& ActorRef, FString& OutError, TArray<FString>* OutMatchedPaths = nullptr);
};
