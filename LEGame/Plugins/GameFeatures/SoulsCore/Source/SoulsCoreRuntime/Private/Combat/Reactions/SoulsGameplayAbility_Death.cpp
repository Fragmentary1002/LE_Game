#include "Combat/Reactions/SoulsGameplayAbility_Death.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/LyraHealthComponent.h"
#include "LyraGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsGameplayAbility_Death)

// 独立激活后再切换阻塞组，确保死亡能先取消正在运行的普通动作。
USoulsGameplayAbility_Death::USoulsGameplayAbility_Death(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ActivationGroup = ELyraAbilityActivationGroup::Independent;
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Souls.Death")));
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        FAbilityTriggerData Trigger;
        Trigger.TriggerTag = LyraGameplayTags::GameplayEvent_Death;
        Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
        AbilityTriggers.Add(Trigger);
    }
}

// 开始死亡时取消其余普通能力，动画完整结束后才调用 FinishDeath。
void USoulsGameplayAbility_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponentFromActorInfo())
    {
        FGameplayTagContainer SurvivesDeath;
        SurvivesDeath.AddTag(LyraGameplayTags::Ability_Behavior_SurvivesDeath);
        ASC->CancelAbilities(nullptr, &SurvivesDeath, this);
    }
    SetCanBeCanceled(false);
    ChangeActivationGroup(ELyraAbilityActivationGroup::Exclusive_Blocking);
    ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo());
    if (Health && Health->GetDeathState() == ELyraDeathState::NotDead)
    {
        Health->StartDeath();
    }
    if (!Health || !DeathMontage || !ActorInfo->GetAnimInstance())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,
        NAME_None, DeathMontage, 1.0f, NAME_None, true);
    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageEnded);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageEnded);
    MontageTask->ReadyForActivation();
}

// 所有结束路径都保证 HealthComponent 完成死亡，避免角色永久停在半死亡状态。
void USoulsGameplayAbility_Death::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (MontageTask)
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
        MontageTask = nullptr;
    }
    if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo()))
    {
        if (Health->GetDeathState() == ELyraDeathState::DeathStarted)
        {
            Health->FinishDeath();
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 死亡动作结束后释放能力并触发角色已有的销毁流程。
void USoulsGameplayAbility_Death::HandleMontageEnded()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}
