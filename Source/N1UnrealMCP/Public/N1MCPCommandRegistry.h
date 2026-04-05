#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FN1MCPCommandEntry
{
	FString CommandName;
	FString Category;
	FString Description;
	TSharedPtr<FJsonObject> ParameterSchema;
	bool bMutatesEditorState = false;
	bool bRequiresPIE = false;
	bool bLongRunning = false;
	int32 DefaultTimeoutMs = 10000;
	TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Handler;
};

class FN1MCPCommandRegistry
{
public:
	FN1MCPCommandRegistry();

	void Register(const FN1MCPCommandEntry& Entry);
	TSharedPtr<FJsonObject> Execute(const FString& Command, const TSharedPtr<FJsonObject>& Params);
	const FN1MCPCommandEntry* Find(const FString& Command) const;
	TArray<FN1MCPCommandEntry> GetAllCommands() const;
	TArray<FN1MCPCommandEntry> GetCommandsByCategory(const FString& Category) const;
	TArray<FString> GetCategories() const;

	static bool ValidateParams(
		const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonObject>& Schema,
		FString& OutError
	);

	static TSharedPtr<FJsonObject> ApplyPagination(
		const TArray<TSharedPtr<FJsonValue>>& AllItems,
		const TSharedPtr<FJsonObject>& Params,
		int32 DefaultLimit = 100,
		int32 MaxLimit = 1000
	);

private:
	TMap<FString, FN1MCPCommandEntry> Commands;

	void RegisterMetaCommands();
	TSharedPtr<FJsonObject> HandleListCommands(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListCategories(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDescribeCommand(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePing(const TSharedPtr<FJsonObject>& Params);
};
