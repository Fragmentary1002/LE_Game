using UnrealBuildTool;

public class SoulsCoreRuntime : ModuleRules
{
    // 声明魂类运行时所需的 Lyra、团队接口、输入、相机与 UI 依赖。
    public SoulsCoreRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "LyraGame", "AIModule", "GameplayAbilities",
            "GameplayTags", "GameplayTasks", "ModularGameplay", "ModularGameplayActors",
            "LEAbilityFramework", "EnhancedInput", "InputCore", "UMG", "CommonUI", "CommonGame"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "Projects" });
    }
}
