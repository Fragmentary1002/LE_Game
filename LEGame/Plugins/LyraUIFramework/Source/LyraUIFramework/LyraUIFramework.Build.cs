using UnrealBuildTool;

public class LyraUIFramework : ModuleRules
{
    public LyraUIFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "UMG",
            "CommonUI",
            "CommonGame",
            "UIExtension"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "DeveloperSettings"
        });
    }
}
