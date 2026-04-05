#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "N1MCPCommandRegistry.h"
#include "N1MCPHandlerBase.h"
#include "N1MCPBridge.generated.h"

class FN1MCPServerRunnable;

UCLASS()
class UN1MCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

private:
	FN1MCPCommandRegistry CommandRegistry;
	TArray<TSharedPtr<FN1MCPHandlerBase>> Handlers;
	TSharedPtr<FN1MCPServerRunnable> ServerRunnable;
	FRunnableThread* ServerThread = nullptr;
	bool bIsRunning = false;

	TSharedPtr<FJsonObject> ExecuteCommand(const FString& Command, const TSharedPtr<FJsonObject>& Params);
	void RegisterAllHandlers();
};
