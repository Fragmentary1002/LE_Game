using UnrealBuildTool;

public class LyraUIFrameworkEditor : ModuleRules
{
    // 配置编辑器资产生成、蓝图检查及右键菜单所需依赖。
    public LyraUIFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AssetRegistry",
            "AssetTools",
            "ContentBrowser",
            "UnrealEd",
            "UMG",
            "UMGEditor",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "LyraUIFramework"
        });
    }
}
