using UnrealBuildTool;

public class LECommonWidget : ModuleRules
{
    // 配置通用控件模块依赖，保证不反向依赖界面流程框架。
    public LECommonWidget(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "CommonUI",
            "SlateCore",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate"
        });
    }
}
