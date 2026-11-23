using UnrealBuildTool;

public class LEAbilityFramework : ModuleRules
{
    // 声明框架公开类型使用的 UE5 输入和技能模块。
    public LEAbilityFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks"
        });
    }
}
