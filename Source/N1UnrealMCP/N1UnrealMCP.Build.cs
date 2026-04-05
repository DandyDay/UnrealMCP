using UnrealBuildTool;

public class N1UnrealMCP : ModuleRules
{
    public N1UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IWYUSupport = IWYUSupport.Full;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Networking",
            "Sockets",
            "HTTP",
            "Json",
            "JsonUtilities",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "EditorScriptingUtilities",
            "EditorSubsystem",
            "Slate",
            "SlateCore",
            "UMG",
            "Kismet",
            "KismetCompiler",
            "BlueprintGraph",
            "Projects",
            "AssetRegistry",
            "PropertyEditor",
            "ToolMenus",
            "BlueprintEditorLibrary",
            "UMGEditor",
            "Landscape",
            "EnhancedInput",
            "EngineSettings"
        });
    }
}
