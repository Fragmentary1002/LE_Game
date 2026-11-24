using UnrealBuildTool;

public class LEAbilityFrameworkEditor : ModuleRules
{
    // 加载技能资产的详情、预览和资产编辑工具。
    public LEAbilityFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GameplayTags", "LEAbilityFramework",
            "AssetTools", "UnrealEd", "EditorFramework", "PropertyEditor",
            "Slate", "SlateCore", "InputCore"
        });
    }
}
