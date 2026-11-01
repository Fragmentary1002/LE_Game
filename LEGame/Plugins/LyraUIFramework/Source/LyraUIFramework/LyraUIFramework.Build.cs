using UnrealBuildTool;

public class LyraUIFramework : ModuleRules
{
    // 配置运行时模块依赖，公开头文件所需的模块也必须公开传递。
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
            "UIExtension",
            "SlateCore",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate"
        });
    }
}
