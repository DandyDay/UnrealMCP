#include "N1UnrealMCPModule.h"

DEFINE_LOG_CATEGORY(LogN1MCP);

#define LOCTEXT_NAMESPACE "FN1UnrealMCPModule"

void FN1UnrealMCPModule::StartupModule()
{
	UE_LOG(LogN1MCP, Log, TEXT("N1UnrealMCP: Module started"));
}

void FN1UnrealMCPModule::ShutdownModule()
{
	UE_LOG(LogN1MCP, Log, TEXT("N1UnrealMCP: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FN1UnrealMCPModule, N1UnrealMCP)
