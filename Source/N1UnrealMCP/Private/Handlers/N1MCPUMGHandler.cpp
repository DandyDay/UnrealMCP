#include "Handlers/N1MCPUMGHandler.h"
#include "N1UnrealMCPModule.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"

FN1MCPUMGHandler::FN1MCPUMGHandler(FN1MCPCommandRegistry& InRegistry)
	: FN1MCPHandlerBase(InRegistry, TEXT("umg"))
{
}

UWidgetBlueprint* FN1MCPUMGHandler::FindWidgetBP(const FString& Path, FString& OutError)
{
	FString AssetPath = Path;
	if (!AssetPath.StartsWith(TEXT("/")))
		AssetPath = TEXT("/Game/Widgets/") + AssetPath;
	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!WBP) OutError = FString::Printf(TEXT("Widget blueprint not found: '%s'"), *AssetPath);
	return WBP;
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::WidgetToJson(UWidget* Widget)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), Widget->GetName());
	Obj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
	Obj->SetBoolField(TEXT("is_visible"), Widget->IsVisible());
	return Obj;
}

void FN1MCPUMGHandler::RegisterCommands()
{
	RegisterCommand(TEXT("create_widget_blueprint"),
		TEXT("위젯 블루프린트 생성"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleCreateWidgetBlueprint(P); });

	RegisterCommand(TEXT("add_widget"),
		TEXT("위젯 블루프린트에 위젯 추가"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleAddWidget(P); });

	RegisterCommand(TEXT("remove_widget"),
		TEXT("위젯 제거"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleRemoveWidget(P); });

	RegisterCommand(TEXT("set_widget_property"),
		TEXT("위젯 속성 수정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetWidgetProperty(P); });

	RegisterCommand(TEXT("set_widget_layout"),
		TEXT("위젯 레이아웃/앵커 설정"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleSetWidgetLayout(P); });

	RegisterCommand(TEXT("bind_widget_event"),
		TEXT("위젯 이벤트 바인딩"), nullptr,
		true, false, false, 10000,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleBindWidgetEvent(P); });

	RegisterCommand(TEXT("get_widget_tree"),
		TEXT("위젯 계층 구조 조회"), nullptr,
		[this](const TSharedPtr<FJsonObject>& P) { return HandleGetWidgetTree(P); });
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::HandleCreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString Name;
	if (!Params || !Params->TryGetStringField(TEXT("name"), Name))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'name' required"));

	FString Path = TEXT("/Game/Widgets");
	Params->TryGetStringField(TEXT("path"), Path);

	FString PackagePath = Path / Name;
	UPackage* Package = CreatePackage(*PackagePath);

	UWidgetBlueprint* WBP = CastChecked<UWidgetBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(
			UUserWidget::StaticClass(), Package, FName(*Name),
			BPTYPE_Normal, UWidgetBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass()));

	if (!WBP)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create widget blueprint"));

	// 루트 Canvas Panel 추가
	UCanvasPanel* RootCanvas = WBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WBP->WidgetTree->RootWidget = RootCanvas;

	FAssetRegistryModule::AssetCreated(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);
	WBP->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset_path"), WBP->GetPathName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::HandleAddWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString WBPPath, WidgetClassStr;
	if (!Params || !Params->TryGetStringField(TEXT("widget_bp_path"), WBPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_bp_path' required"));
	if (!Params->TryGetStringField(TEXT("widget_class"), WidgetClassStr))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_class' required"));

	FString Error;
	UWidgetBlueprint* WBP = FindWidgetBP(WBPPath, Error);
	if (!WBP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UClass* WidgetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/UMG.%s"), *WidgetClassStr));
	if (!WidgetClass)
		WidgetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/UMG.U%s"), *WidgetClassStr));
	if (!WidgetClass)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Widget class '%s' not found"), *WidgetClassStr));

	FString WidgetName = WidgetClassStr;
	Params->TryGetStringField(TEXT("name"), WidgetName);

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: add_widget")));

	UWidget* NewWidget = WBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
	if (!NewWidget)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Failed to create widget"));

	// 루트에 추가
	UPanelWidget* Root = Cast<UPanelWidget>(WBP->WidgetTree->RootWidget);
	if (Root)
	{
		Root->AddChild(NewWidget);
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget_name"), NewWidget->GetName());
	Data->SetStringField(TEXT("widget_class"), WidgetClass->GetName());
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::HandleRemoveWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString WBPPath, WidgetName;
	if (!Params || !Params->TryGetStringField(TEXT("widget_bp_path"), WBPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_bp_path' required"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_name' required"));

	FString Error;
	UWidgetBlueprint* WBP = FindWidgetBP(WBPPath, Error);
	if (!WBP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UWidget* Widget = WBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
		return ErrorResponse(TEXT("WIDGET_NOT_FOUND"),
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: remove_widget")));
	WBP->WidgetTree->RemoveWidget(Widget);
	FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString WBPPath, WidgetName, PropName;
	if (!Params || !Params->TryGetStringField(TEXT("widget_bp_path"), WBPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_bp_path' required"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_name' required"));
	if (!Params->TryGetStringField(TEXT("property"), PropName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'property' required"));

	FString Error;
	UWidgetBlueprint* WBP = FindWidgetBP(WBPPath, Error);
	if (!WBP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UWidget* Widget = WBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
		return ErrorResponse(TEXT("WIDGET_NOT_FOUND"),
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	// TextBlock 특수 처리: Text 속성
	if (PropName == TEXT("Text"))
	{
		UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
		if (TextBlock)
		{
			FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_widget_property")));
			TextBlock->SetText(FText::FromString(Params->GetStringField(TEXT("value"))));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);

			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetBoolField(TEXT("success"), true);
			return SuccessResponse(Data);
		}
	}

	// 일반 리플렉션
	FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropName));
	if (!Prop)
		return ErrorResponse(TEXT("INVALID_PARAMS"),
			FString::Printf(TEXT("Property '%s' not found"), *PropName));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_widget_property")));
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Widget);

	if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))
		BP->SetPropertyValue(PropAddr, Value->AsBool());
	else if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
		FP->SetPropertyValue(PropAddr, static_cast<float>(Value->AsNumber()));
	else if (FStrProperty* SP = CastField<FStrProperty>(Prop))
		SP->SetPropertyValue(PropAddr, Value->AsString());

	FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::HandleSetWidgetLayout(const TSharedPtr<FJsonObject>& Params)
{
	FString WBPPath, WidgetName;
	if (!Params || !Params->TryGetStringField(TEXT("widget_bp_path"), WBPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_bp_path' required"));
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_name' required"));

	FString Error;
	UWidgetBlueprint* WBP = FindWidgetBP(WBPPath, Error);
	if (!WBP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	UWidget* Widget = WBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
		return ErrorResponse(TEXT("WIDGET_NOT_FOUND"),
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));

	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!Slot)
		return ErrorResponse(TEXT("INTERNAL_ERROR"), TEXT("Widget is not in a CanvasPanel"));

	FScopedTransaction Transaction(FText::FromString(TEXT("N1UnrealMCP: set_widget_layout")));

	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (Params->TryGetArrayField(TEXT("position"), Arr) && Arr->Num() >= 2)
	{
		Slot->SetPosition(FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber()));
	}
	if (Params->TryGetArrayField(TEXT("size"), Arr) && Arr->Num() >= 2)
	{
		Slot->SetSize(FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber()));
	}
	if (Params->TryGetArrayField(TEXT("alignment"), Arr) && Arr->Num() >= 2)
	{
		Slot->SetAlignment(FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber()));
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), true);
	return SuccessResponse(Data);
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params)
{
	// v1: 이벤트 바인딩은 BP 그래프 조작이 필요하므로 플래그만 설정
	return ErrorResponse(TEXT("INTERNAL_ERROR"),
		TEXT("bind_widget_event is not yet implemented in v1 - use Blueprint Node commands to create event bindings"));
}

TSharedPtr<FJsonObject> FN1MCPUMGHandler::HandleGetWidgetTree(const TSharedPtr<FJsonObject>& Params)
{
	FString WBPPath;
	if (!Params || !Params->TryGetStringField(TEXT("widget_bp_path"), WBPPath))
		return ErrorResponse(TEXT("INVALID_PARAMS"), TEXT("'widget_bp_path' required"));

	FString Error;
	UWidgetBlueprint* WBP = FindWidgetBP(WBPPath, Error);
	if (!WBP) return ErrorResponse(TEXT("ASSET_NOT_FOUND"), Error);

	TFunction<TSharedPtr<FJsonObject>(UWidget*)> SerializeWidget;
	SerializeWidget = [&](UWidget* Widget) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> Obj = WidgetToJson(Widget);

		UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
		if (Panel)
		{
			TArray<TSharedPtr<FJsonValue>> Children;
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				UWidget* Child = Panel->GetChildAt(i);
				if (Child)
					Children.Add(MakeShared<FJsonValueObject>(SerializeWidget(Child)));
			}
			Obj->SetArrayField(TEXT("children"), Children);
		}
		return Obj;
	};

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget_blueprint"), WBP->GetPathName());

	if (WBP->WidgetTree->RootWidget)
	{
		Data->SetObjectField(TEXT("tree"), SerializeWidget(WBP->WidgetTree->RootWidget));
	}

	return SuccessResponse(Data);
}
