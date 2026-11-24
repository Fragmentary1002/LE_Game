#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "SoulsGameplayAbility_LightAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class ULECombatTimelineTask;
struct FLEAbilityInteraction;
class USoulsCombatActionComponent;
class USoulsMeleeTraceComponent;

/** 单次轻攻击能力，拥有动作、有效窗口、命中伤害和取消清理的完整生命周期。 */
UCLASS(Blueprintable)
class SOULSCORERUNTIME_API USoulsGameplayAbility_LightAttack : public ULyraGameplayAbility
{
    GENERATED_BODY()

public:
    // 按下一次启动一次，禁止攻击重复激活并允许受击与死亡取消。
    USoulsGameplayAbility_LightAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
    // 仅在攻击配置、动画、武器和权威角色全部就绪时允许出招。
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

    // 提交能力并启动 Montage 和本次招式配置的时间轴。
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // 所有结束路径均释放检测、绑定和临时移动限制。
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    // 为每个有效目标生成独立伤害上下文并在权威侧施加 GE。
    void HandleConfirmedHit(const FHitResult& Hit);

    // 完整动作播放完毕时结束本次攻击。
    UFUNCTION()
    void HandleMontageCompleted();

    // 播放失败或动作中断时立即取消攻击并关闭伤害窗口。
    UFUNCTION()
    void HandleMontageInterrupted();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    // 把通用命中窗口和派生窗口分发给 Souls 组件。
    void HandleTimelineWindow(FGameplayTag Tag, bool bOpen);

    // 将配置中的点事件发送到当前角色的 ASC。
    void HandleTimelinePoint(FGameplayTag Tag);

    UPROPERTY(Transient)
    TObjectPtr<ULECombatTimelineTask> TimelineTask;

    // 在权威侧应用技能资产的标签交互规则。
    void ApplyInteraction(const FLEAbilityInteraction& Interaction);

    FGameplayTagContainer InteractionOwnedTags;
    FGameplayTagContainer InteractionBlockedAbilities;
    TWeakObjectPtr<USoulsCombatActionComponent> ActiveCombat;
    TWeakObjectPtr<USoulsMeleeTraceComponent> ActiveTrace;
    FDelegateHandle HitDelegateHandle;
};
