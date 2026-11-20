#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SoulsGameplayAbility_HitReact.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/** 按来袭方向播放硬直，普通攻击可被硬直和死亡打断。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsGameplayAbility_HitReact : public ULyraGameplayAbility
{
    GENERATED_BODY()

public:
    // 配置事件触发、硬直标签和攻击取消规则。
    USoulsGameplayAbility_HitReact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|HitReact")
    TObjectPtr<UAnimMontage> FrontMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|HitReact")
    TObjectPtr<UAnimMontage> BackMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|HitReact")
    TObjectPtr<UAnimMontage> LeftMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|HitReact")
    TObjectPtr<UAnimMontage> RightMontage;

protected:
    // 从命中来源选择动作并开始本次受击。
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    // 恢复非死亡角色的移动并释放动画任务。
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    // 根据攻击来源与角色朝向选择四向受击动画。
    UAnimMontage* SelectMontage(const FGameplayEventData* EventData) const;
    // 动画自然结束或异常中断后结束能力。
    UFUNCTION()
    void HandleMontageEnded();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
    TWeakObjectPtr<UCharacterMovementComponent> LockedMovement;
    TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_Walking;
    uint8 PreviousCustomMode = 0;
};
