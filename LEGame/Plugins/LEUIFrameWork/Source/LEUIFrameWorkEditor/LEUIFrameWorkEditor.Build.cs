using UnrealBuildTool;

public class LEUIFrameWorkEditor : ModuleRules
{
    // 配置编辑器资产生成、蓝图检查及右键菜单所需依赖。
    public LEUIFrameWorkEditor(ReadOnlyTargetRules Target) : base(Target)
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
            "CommonUI",
            "UMGEditor",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "LEUIFrameWork"
        });
    }
}
