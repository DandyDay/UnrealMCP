#include "N1MCPBridge.h"
#include "N1MCPServerRunnable.h"
#include "N1UnrealMCPModule.h"
#include "Handlers/N1MCPEditorHandler.h"
#include "Handlers/N1MCPBlueprintHandler.h"
#include "Handlers/N1MCPBlueprintNodeHandler.h"
#include "Handlers/N1MCPMaterialHandler.h"
#include "Handlers/N1MCPUMGHandler.h"
#include "Handlers/N1MCPProjectHandler.h"
#include "Handlers/N1MCPAssetHandler.h"
#include "Handlers/N1MCPLandscapeHandler.h"
#include "Handlers/N1MCPPIEHandler.h"
#include "Handlers/N1MCPDataHandler.h"

void UN1MCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterAllHandlers();
	StartServer();
}

void UN1MCPBridge::Deinitialize()
{
	StopServer();
	Super::Deinitialize();
}

void UN1MCPBridge::RegisterAllHandlers()
{
	// 메타 커맨드는 CommandRegistry 생성자에서 자동 등록됨

	auto EditorHandler = MakeShared<FN1MCPEditorHandler>(CommandRegistry);
	EditorHandler->RegisterCommands();
	Handlers.Add(EditorHandler);

	auto BlueprintHandler = MakeShared<FN1MCPBlueprintHandler>(CommandRegistry);
	BlueprintHandler->RegisterCommands();
	Handlers.Add(BlueprintHandler);

	auto BPNodeHandler = MakeShared<FN1MCPBlueprintNodeHandler>(CommandRegistry);
	BPNodeHandler->RegisterCommands();
	Handlers.Add(BPNodeHandler);

	auto MaterialHandler = MakeShared<FN1MCPMaterialHandler>(CommandRegistry);
	MaterialHandler->RegisterCommands();
	Handlers.Add(MaterialHandler);

	auto UMGHandler = MakeShared<FN1MCPUMGHandler>(CommandRegistry);
	UMGHandler->RegisterCommands();
	Handlers.Add(UMGHandler);

	auto ProjectHandler = MakeShared<FN1MCPProjectHandler>(CommandRegistry);
	ProjectHandler->RegisterCommands();
	Handlers.Add(ProjectHandler);

	auto AssetHandler = MakeShared<FN1MCPAssetHandler>(CommandRegistry);
	AssetHandler->RegisterCommands();
	Handlers.Add(AssetHandler);

	auto LandscapeHandler = MakeShared<FN1MCPLandscapeHandler>(CommandRegistry);
	LandscapeHandler->RegisterCommands();
	Handlers.Add(LandscapeHandler);

	auto PIEHandler = MakeShared<FN1MCPPIEHandler>(CommandRegistry);
	PIEHandler->RegisterCommands();
	Handlers.Add(PIEHandler);

	auto DataHandler = MakeShared<FN1MCPDataHandler>(CommandRegistry);
	DataHandler->RegisterCommands();
	Handlers.Add(DataHandler);
}

void UN1MCPBridge::StartServer()
{
	if (bIsRunning) return;

	FOnMCPCommandReceived Delegate;
	Delegate.BindUObject(this, &UN1MCPBridge::ExecuteCommand);

	ServerRunnable = MakeShared<FN1MCPServerRunnable>(55558, Delegate);
	ServerThread = FRunnableThread::Create(
		ServerRunnable.Get(), TEXT("N1MCPServerThread"));
	bIsRunning = true;

	UE_LOG(LogN1MCP, Log, TEXT("N1UnrealMCP Bridge started"));
}

void UN1MCPBridge::StopServer()
{
	if (!bIsRunning) return;

	if (ServerRunnable)
	{
		ServerRunnable->Stop();
	}
	if (ServerThread)
	{
		ServerThread->WaitForCompletion();
		delete ServerThread;
		ServerThread = nullptr;
	}
	ServerRunnable.Reset();
	bIsRunning = false;

	UE_LOG(LogN1MCP, Log, TEXT("N1UnrealMCP Bridge stopped"));
}

TSharedPtr<FJsonObject> UN1MCPBridge::ExecuteCommand(
	const FString& Command, const TSharedPtr<FJsonObject>& Params)
{
	return CommandRegistry.Execute(Command, Params);
}
