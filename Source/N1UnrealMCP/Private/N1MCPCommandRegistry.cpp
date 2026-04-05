#include "N1MCPCommandRegistry.h"
#include "N1UnrealMCPModule.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"

FN1MCPCommandRegistry::FN1MCPCommandRegistry()
{
	RegisterMetaCommands();
}

void FN1MCPCommandRegistry::Register(const FN1MCPCommandEntry& Entry)
{
	if (Commands.Contains(Entry.CommandName))
	{
		UE_LOG(LogN1MCP, Error,
			TEXT("Command '%s' already registered - ignoring duplicate"),
			*Entry.CommandName);
		return;
	}
	Commands.Add(Entry.CommandName, Entry);
}

const FN1MCPCommandEntry* FN1MCPCommandRegistry::Find(const FString& Command) const
{
	return Commands.Find(Command);
}

TSharedPtr<FJsonObject> FN1MCPCommandRegistry::Execute(
	const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
	const FN1MCPCommandEntry* Entry = Find(Command);
	if (!Entry)
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("status"), TEXT("error"));
		Error->SetStringField(TEXT("error_code"), TEXT("COMMAND_NOT_FOUND"));
		Error->SetStringField(TEXT("error"),
			FString::Printf(TEXT("Command '%s' not found"), *Command));
		return Error;
	}

	if (Entry->bRequiresPIE)
	{
		if (!GEditor || !GEditor->PlayWorld)
		{
			TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
			Error->SetStringField(TEXT("status"), TEXT("error"));
			Error->SetStringField(TEXT("error_code"), TEXT("PIE_NOT_RUNNING"));
			Error->SetStringField(TEXT("error"),
				TEXT("This command requires PIE to be running"));
			return Error;
		}
	}

	if (Entry->ParameterSchema.IsValid())
	{
		FString ValidationError;
		if (!ValidateParams(Params, Entry->ParameterSchema, ValidationError))
		{
			TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
			Error->SetStringField(TEXT("status"), TEXT("error"));
			Error->SetStringField(TEXT("error_code"), TEXT("INVALID_PARAMS"));
			Error->SetStringField(TEXT("error"), ValidationError);
			return Error;
		}
	}

	return Entry->Handler(Params);
}

bool FN1MCPCommandRegistry::ValidateParams(
	const TSharedPtr<FJsonObject>& Params,
	const TSharedPtr<FJsonObject>& Schema,
	FString& OutError)
{
	if (!Schema.IsValid()) return true;

	const TArray<TSharedPtr<FJsonValue>>* RequiredArr;
	if (Schema->TryGetArrayField(TEXT("required"), RequiredArr))
	{
		for (const auto& ReqVal : *RequiredArr)
		{
			FString FieldName = ReqVal->AsString();
			if (!Params.IsValid() || !Params->HasField(FieldName))
			{
				OutError = FString::Printf(
					TEXT("Missing required parameter: '%s'"), *FieldName);
				return false;
			}
		}
	}

	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (Schema->TryGetObjectField(TEXT("properties"), PropertiesObj) && Params.IsValid())
	{
		for (const auto& PropPair : (*PropertiesObj)->Values)
		{
			const FString& FieldName = PropPair.Key;
			if (!Params->HasField(FieldName)) continue;

			const TSharedPtr<FJsonObject>* PropSchema;
			if (!PropPair.Value->TryGetObject(PropSchema)) continue;

			FString ExpectedType;
			if (!(*PropSchema)->TryGetStringField(TEXT("type"), ExpectedType)) continue;

			TSharedPtr<FJsonValue> ParamVal = Params->TryGetField(FieldName);
			if (!ParamVal.IsValid()) continue;

			bool bTypeMatch = false;
			if (ExpectedType == TEXT("string"))
				bTypeMatch = (ParamVal->Type == EJson::String);
			else if (ExpectedType == TEXT("number"))
				bTypeMatch = (ParamVal->Type == EJson::Number);
			else if (ExpectedType == TEXT("boolean"))
				bTypeMatch = (ParamVal->Type == EJson::Boolean);
			else if (ExpectedType == TEXT("array"))
				bTypeMatch = (ParamVal->Type == EJson::Array);
			else if (ExpectedType == TEXT("object"))
				bTypeMatch = (ParamVal->Type == EJson::Object);
			else
				bTypeMatch = true;

			if (!bTypeMatch)
			{
				OutError = FString::Printf(
					TEXT("Parameter '%s' expected type '%s'"),
					*FieldName, *ExpectedType);
				return false;
			}
		}
	}

	return true;
}

TArray<FN1MCPCommandEntry> FN1MCPCommandRegistry::GetAllCommands() const
{
	TArray<FN1MCPCommandEntry> Result;
	Commands.GenerateValueArray(Result);
	return Result;
}

TArray<FN1MCPCommandEntry> FN1MCPCommandRegistry::GetCommandsByCategory(
	const FString& Category) const
{
	TArray<FN1MCPCommandEntry> Result;
	for (const auto& Pair : Commands)
	{
		if (Pair.Value.Category == Category)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<FString> FN1MCPCommandRegistry::GetCategories() const
{
	TSet<FString> Categories;
	for (const auto& Pair : Commands)
	{
		Categories.Add(Pair.Value.Category);
	}
	return Categories.Array();
}

TSharedPtr<FJsonObject> FN1MCPCommandRegistry::ApplyPagination(
	const TArray<TSharedPtr<FJsonValue>>& AllItems,
	const TSharedPtr<FJsonObject>& Params,
	int32 DefaultLimit,
	int32 MaxLimit)
{
	int32 Limit = DefaultLimit;
	int32 Offset = 0;
	if (Params.IsValid())
	{
		if (Params->HasField(TEXT("limit")))
		{
			Limit = FMath::Clamp(
				static_cast<int32>(Params->GetNumberField(TEXT("limit"))),
				1, MaxLimit);
		}
		if (Params->HasField(TEXT("offset")))
		{
			Offset = FMath::Max(0,
				static_cast<int32>(Params->GetNumberField(TEXT("offset"))));
		}
	}

	int32 Total = AllItems.Num();
	int32 Start = FMath::Min(Offset, Total);
	int32 End = FMath::Min(Start + Limit, Total);

	TArray<TSharedPtr<FJsonValue>> PageItems;
	for (int32 i = Start; i < End; ++i)
	{
		PageItems.Add(AllItems[i]);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("items"), PageItems);
	Data->SetNumberField(TEXT("total"), Total);
	Data->SetNumberField(TEXT("limit"), Limit);
	Data->SetNumberField(TEXT("offset"), Offset);
	Data->SetBoolField(TEXT("has_more"), End < Total);
	return Data;
}

void FN1MCPCommandRegistry::RegisterMetaCommands()
{
	Register({
		TEXT("list_commands"), TEXT("meta"),
		TEXT("등록된 모든 커맨드 이름과 설명을 반환"),
		nullptr, false, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleListCommands(P); }
	});
	Register({
		TEXT("list_categories"), TEXT("meta"),
		TEXT("카테고리 목록 반환"),
		nullptr, false, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleListCategories(P); }
	});
	Register({
		TEXT("describe_command"), TEXT("meta"),
		TEXT("특정 커맨드의 상세 파라미터 스키마 반환"),
		nullptr, false, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleDescribeCommand(P); }
	});
	Register({
		TEXT("ping"), TEXT("meta"),
		TEXT("연결 확인"),
		nullptr, false, false, false, 5000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandlePing(P); }
	});
}

TSharedPtr<FJsonObject> FN1MCPCommandRegistry::HandleListCommands(
	const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> CommandArray;
	for (const auto& Pair : Commands)
	{
		TSharedPtr<FJsonObject> Cmd = MakeShared<FJsonObject>();
		Cmd->SetStringField(TEXT("name"), Pair.Value.CommandName);
		Cmd->SetStringField(TEXT("category"), Pair.Value.Category);
		Cmd->SetStringField(TEXT("description"), Pair.Value.Description);
		CommandArray.Add(MakeShared<FJsonValueObject>(Cmd));
	}

	TSharedPtr<FJsonObject> Data = ApplyPagination(CommandArray, Params);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("success"));
	Result->SetObjectField(TEXT("result"), Data);
	return Result;
}

TSharedPtr<FJsonObject> FN1MCPCommandRegistry::HandleListCategories(
	const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> CatArray;
	for (const FString& Cat : GetCategories())
	{
		CatArray.Add(MakeShared<FJsonValueString>(Cat));
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("success"));
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("categories"), CatArray);
	Result->SetObjectField(TEXT("result"), Data);
	return Result;
}

TSharedPtr<FJsonObject> FN1MCPCommandRegistry::HandleDescribeCommand(
	const TSharedPtr<FJsonObject>& Params)
{
	FString CmdName;
	if (!Params || !Params->TryGetStringField(TEXT("command"), CmdName))
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("status"), TEXT("error"));
		Error->SetStringField(TEXT("error_code"), TEXT("INVALID_PARAMS"));
		Error->SetStringField(TEXT("error"), TEXT("'command' parameter required"));
		return Error;
	}

	const FN1MCPCommandEntry* Entry = Find(CmdName);
	if (!Entry)
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("status"), TEXT("error"));
		Error->SetStringField(TEXT("error_code"), TEXT("COMMAND_NOT_FOUND"));
		Error->SetStringField(TEXT("error"),
			FString::Printf(TEXT("Command '%s' not found"), *CmdName));
		return Error;
	}

	TSharedPtr<FJsonObject> CmdInfo = MakeShared<FJsonObject>();
	CmdInfo->SetStringField(TEXT("command"), Entry->CommandName);
	CmdInfo->SetStringField(TEXT("category"), Entry->Category);
	CmdInfo->SetStringField(TEXT("description"), Entry->Description);
	if (Entry->ParameterSchema.IsValid())
	{
		CmdInfo->SetObjectField(TEXT("parameter_schema"), Entry->ParameterSchema);
	}
	CmdInfo->SetBoolField(TEXT("mutates_editor_state"), Entry->bMutatesEditorState);
	CmdInfo->SetBoolField(TEXT("requires_pie"), Entry->bRequiresPIE);
	CmdInfo->SetBoolField(TEXT("long_running"), Entry->bLongRunning);
	CmdInfo->SetNumberField(TEXT("default_timeout_ms"), Entry->DefaultTimeoutMs);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("success"));
	Result->SetObjectField(TEXT("result"), CmdInfo);
	return Result;
}

TSharedPtr<FJsonObject> FN1MCPCommandRegistry::HandlePing(
	const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("success"));
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("pong"), TEXT("N1UnrealMCP"));
	Data->SetStringField(TEXT("version"), TEXT("1.0.0"));
	Result->SetObjectField(TEXT("result"), Data);
	return Result;
}
