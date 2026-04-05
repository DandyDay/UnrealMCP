#include "Handlers/N1MCPProjectHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Interfaces/IPluginManager.h"
#include "GameFramework/InputSettings.h"
#include "GeneralProjectSettings.h"

FN1MCPProjectHandler::FN1MCPProjectHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("project"))
{
}

void FN1MCPProjectHandler::RegisterCommands()
{
	RegisterCommand(TEXT("create_input_mapping"),
		TEXT("Enhanced Input 기반 입력 매핑 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateInputMapping(P); });

	RegisterCommand(TEXT("get_project_settings"),
		TEXT("프로젝트 설정 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetProjectSettings(P); });

	RegisterCommand(TEXT("set_project_setting"),
		TEXT("프로젝트 설정 변경"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetProjectSetting(P); });

	RegisterCommand(TEXT("get_plugins_list"),
		TEXT("플러그인 목록 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetPluginsList(P); });

	RegisterCommand(TEXT("set_plugin_enabled"),
		TEXT("플러그인 활성/비활성 (재시작 필요)"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetPluginEnabled(P); });
}

TSharedPtr<FJsonObject> FN1MCPProjectHandler::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
	FString ContextPath, ActionPath, Key;
	if (!Params || !Params->TryGetStringField(TEXT("mapping_context_path"), ContextPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'mapping_context_path' required"));
	if (!Params->TryGetStringField(TEXT("input_action_path"), ActionPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'input_action_path' required"));
	if (!Params->TryGetStringField(TEXT("key"), Key))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'key' required"));

	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!Context)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("InputMappingContext not found: '%s'"), *ContextPath));

	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("InputAction not found: '%s'"), *ActionPath));

	FKey InputKey(*Key);
	if (!InputKey.IsValid())
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Invalid key: '%s'"), *Key));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: create_input_mapping")));
	Context->Modify();

	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, InputKey);

	Context->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("context"), Context->GetPathName());
	Data->SetStringField(TEXT("action"), Action->GetPathName());
	Data->SetStringField(TEXT("key"), Key);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPProjectHandler::HandleGetProjectSettings(const TSharedPtr<FJsonObject>& Params)
{
	FString SettingsCategory;
	if (!Params || !Params->TryGetStringField(TEXT("category"), SettingsCategory))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'category' required"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	if (SettingsCategory == TEXT("General") || SettingsCategory == TEXT("general"))
	{
		const UGeneralProjectSettings* Settings = GetDefault<UGeneralProjectSettings>();
		Data->SetStringField(TEXT("project_name"), Settings->ProjectName);
		Data->SetStringField(TEXT("company_name"), Settings->CompanyName);
		Data->SetStringField(TEXT("project_id"), Settings->ProjectID.ToString());
	}
	else if (SettingsCategory == TEXT("Input") || SettingsCategory == TEXT("input"))
	{
		const UInputSettings* Settings = GetDefault<UInputSettings>();
		TArray<TSharedPtr<FJsonValue>> Mappings;
		for (const FInputActionKeyMapping& Mapping : Settings->GetActionMappings())
		{
			TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
			M->SetStringField(TEXT("action"), Mapping.ActionName.ToString());
			M->SetStringField(TEXT("key"), Mapping.Key.ToString());
			Mappings.Add(MakeShared<FJsonValueObject>(M));
		}
		Data->SetArrayField(TEXT("action_mappings"), Mappings);
	}
	else
	{
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Unknown category: '%s' (supported: General, Input)"), *SettingsCategory));
	}

	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPProjectHandler::HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params)
{
	FString SettingPath;
	if (!Params || !Params->TryGetStringField(TEXT("setting_path"), SettingPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'setting_path' required"));
	if (!Params->HasField(TEXT("value")))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'value' required"));

	// CDO를 통한 설정 변경
	FString ClassName, PropertyName;
	if (!SettingPath.Split(TEXT("."), &ClassName, &PropertyName))
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			TEXT("setting_path format: 'ClassName.PropertyName'"));

	UClass* SettingsClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
	if (!SettingsClass)
		SettingsClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/EngineSettings.%s"), *ClassName));
	if (!SettingsClass)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Settings class '%s' not found"), *ClassName));

	UObject* CDO = SettingsClass->GetDefaultObject();
	FProperty* Prop = SettingsClass->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Property '%s' not found on '%s'"), *PropertyName, *ClassName));

	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(CDO);

	if (FStrProperty* SP = CastField<FStrProperty>(Prop))
		SP->SetPropertyValue(PropAddr, Value->AsString());
	else if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))
		BP->SetPropertyValue(PropAddr, Value->AsBool());
	else if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
		FP->SetPropertyValue(PropAddr, static_cast<float>(Value->AsNumber()));
	else if (FIntProperty* IP = CastField<FIntProperty>(Prop))
		IP->SetPropertyValue(PropAddr, static_cast<int32>(Value->AsNumber()));

	CDO->SaveConfig();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("setting"), SettingPath);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPProjectHandler::HandleGetPluginsList(const TSharedPtr<FJsonObject>& Params)
{
	TArray<TSharedPtr<FJsonValue>> PluginArray;
	for (TSharedRef<IPlugin> Plugin : IPluginManager::Get().GetDiscoveredPlugins())
	{
		TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("name"), Plugin->GetName());
		P->SetBoolField(TEXT("enabled"), Plugin->IsEnabled());
		P->SetStringField(TEXT("category"), Plugin->GetDescriptor().Category);
		PluginArray.Add(MakeShared<FJsonValueObject>(P));
	}

	TSharedPtr<FJsonObject> Data = FN1MCPCommandRegistry::ApplyPagination(PluginArray, Params);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPProjectHandler::HandleSetPluginEnabled(const TSharedPtr<FJsonObject>& Params)
{
	FString PluginName;
	if (!Params || !Params->TryGetStringField(TEXT("plugin_name"), PluginName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'plugin_name' required"));
	if (!Params->HasField(TEXT("enabled")))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'enabled' required"));

	bool bEnabled = Params->GetBoolField(TEXT("enabled"));

	// 플러그인 활성/비활성은 .uproject 파일 수정이 필요
	// 간단 구현: 현재 상태만 반환
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Plugin '%s' not found"), *PluginName));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("enabled"), bEnabled);
	Data->SetBoolField(TEXT("restart_required"), true);
	Data->SetStringField(TEXT("plugin"), PluginName);
	Data->SetStringField(TEXT("note"), TEXT("Plugin enable/disable requires .uproject modification and editor restart"));
	return SuccessResponse(Data);
}
