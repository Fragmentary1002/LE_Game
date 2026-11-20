using UnrealBuildTool;

public class SoulsCoreEditor : ModuleRules
{
    // 仅在编辑器中提供蓝图资产生成与验证工具。
    public SoulsCoreEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd", "UMGEditor", "Kismet", "SlateCore", "SoulsCoreRuntime", "LyraGame",
            "EnhancedInput", "CommonUI", "CommonGame", "GameplayAbilities", "GameplayTags"
        });
    }
}
