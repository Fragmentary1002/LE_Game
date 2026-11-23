#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "SoulsGameplayAbility_Death.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/** 保留 Lyra 死亡事件语义，并在死亡动画结束后完成角色清理。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsGameplayAbility_Death : public ULyraGameplayAbility
{
    GENERATED_BODY()

public:
    // 使用标准死亡事件触发，并配置服务端发起的执行方式。
    USoulsGameplayAbility_Death(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Death")
    TObjectPtr<UAnimMontage> DeathMontage;

protected:
    // 取消普通能力、进入死亡状态并播放死亡动作。
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    // 动画结束或能力移除时保证死亡状态完成。
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    // 动画结束与异常中断共用死亡收尾入口。
    UFUNCTION()
    void HandleMontageEnded();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
