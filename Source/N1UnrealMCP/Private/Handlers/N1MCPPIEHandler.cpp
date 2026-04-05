#include "Handlers/N1MCPPIEHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "Components/Widget.h"

TArray<FString> FN1MCPPIEHandler::BlockedPrefixes = {
	TEXT("quit"), TEXT("exit"), TEXT("open"), TEXT("travel"),
	TEXT("servertravel"), TEXT("disconnect"), TEXT("restartlevel"),
	TEXT("py"), TEXT("ce"), TEXT("ke"), TEXT("obj"), TEXT("gc"),
	TEXT("exec"), TEXT("debug crash")
};

FN1MCPPIEHandler::FN1MCPPIEHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("pie"))
{
}

bool FN1MCPPIEHandler::IsCommandBlocked(const FString& Command) const
{
	FString Trimmed = Command.TrimStartAndEnd().ToLower();
	for (const FString& Prefix : BlockedPrefixes)
	{
		if (Trimmed.StartsWith(Prefix))
			return true;
	}
	return false;
}

void FN1MCPPIEHandler::RegisterCommands()
{
	RegisterCommand(TEXT("play_in_editor"),
		TEXT("PIE 시작"), nullptr,
		false, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandlePlayInEditor(P); });

	RegisterCommand(TEXT("stop_pie"),
		TEXT("PIE 중지"), nullptr,
		false, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleStopPIE(P); });

	RegisterCommand(TEXT("is_pie_running"),
		TEXT("PIE 실행 상태 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleIsPIERunning(P); });

	RegisterCommand(TEXT("execute_console_command"),
		TEXT("PIE 콘솔 커맨드 실행 (차단 리스트 적용)"), nullptr,
		false, true, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleExecuteConsoleCommand(P); });

	RegisterCommand(TEXT("add_widget_to_viewport"),
		TEXT("PIE 런타임에 위젯 추가 (PIE 실행 필요)"), nullptr,
		false, true, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddWidgetToViewport(P); });
}

TSharedPtr<FJsonObject> FN1MCPPIEHandler::HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor"));

	if (GEditor->PlayWorld)
		return ErrorResponse(TEXT("PIE_ALREADY_RUNNING"), TEXT("PIE is already running"));

	FRequestPlaySessionParams PlayParams;
	GEditor->RequestPlaySession(PlayParams);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("status"), TEXT("play_requested"));
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPPIEHandler::HandleStopPIE(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No editor"));

	if (!GEditor->PlayWorld)
		return ErrorResponse(TEXT("PIE_NOT_RUNNING"), TEXT("PIE is not running"));

	GEditor->RequestEndPlayMap();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("status"), TEXT("stop_requested"));
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPPIEHandler::HandleIsPIERunning(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("is_running"), GEditor && GEditor->PlayWorld != nullptr);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPPIEHandler::HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
	FString Command;
	if (!Params || !Params->TryGetStringField(TEXT("command"), Command))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'command' required"));

	if (IsCommandBlocked(Command))
		return ErrorResponse(TEXT("PERMISSION_DENIED"),
			FString::Printf(TEXT("Command '%s' is blocked by security policy"), *Command));

	if (!GEditor || !GEditor->PlayWorld)
		return ErrorResponse(TEXT("PIE_NOT_RUNNING"), TEXT("PIE is not running"));

	GEditor->PlayWorld->Exec(GEditor->PlayWorld, *Command);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("command"), Command);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPPIEHandler::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	FString WBPPath;
	if (!Params || !Params->TryGetStringField(TEXT("widget_bp_path"), WBPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_bp_path' required"));

	if (!GEditor || !GEditor->PlayWorld)
		return ErrorResponse(TEXT("PIE_NOT_RUNNING"), TEXT("PIE is not running"));

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *WBPPath);
	if (!WBP)
		return ErrorResponse(TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Widget blueprint not found: '%s'"), *WBPPath));

	UClass* WidgetClass = WBP->GeneratedClass;
	if (!WidgetClass)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Widget blueprint has no generated class"));

	int32 PlayerIndex = 0;
	if (Params->HasField(TEXT("player_index")))
		PlayerIndex = static_cast<int32>(Params->GetNumberField(TEXT("player_index")));

	APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController();
	if (!PC)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("No player controller in PIE"));

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!Widget)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create widget"));

	Widget->AddToViewport();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	Data->SetStringField(TEXT("widget"), WBPPath);
	return SuccessResponse(Data);
}
