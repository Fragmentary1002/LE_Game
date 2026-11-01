using UnrealBuildTool;

public class LyraUIFrameworkEditor : ModuleRules
{
    public LyraUIFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "UMG",
            "UMGEditor",
            "Blutility",
            "ToolMenus",
            "LyraUIFramework"
        });
    }
}
