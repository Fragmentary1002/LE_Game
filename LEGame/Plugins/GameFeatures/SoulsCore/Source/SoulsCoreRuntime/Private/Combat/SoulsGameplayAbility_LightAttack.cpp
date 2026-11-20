#include "Combat/SoulsGameplayAbility_LightAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/SoulsAttackDefinition.h"
#include "Combat/SoulsCombatActionComponent.h"
#include "Combat/SoulsMeleeTraceComponent.h"
#include "Combat/SoulsMeleeWeapon.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsGameplayAbility_LightAttack)

// 能力互斥由标签表达，避免独占组阻止受击能力进入。
USoulsGameplayAbility_LightAttack::USoulsGameplayAbility_LightAttack(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
    ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;
    ActivationGroup = ELyraAbilityActivationGroup::Independent;
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Souls.LightAttack")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.Attacking")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.Attacking")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Death")));
    FAbilityTriggerData LightAttackTrigger;
    LightAttackTrigger.TriggerTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Souls.LightAttack"));
    LightAttackTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(LightAttackTrigger);
}

// 资产不完整或角色已经死亡时拒绝激活，不留半初始化的攻击状态。
bool USoulsGameplayAbility_LightAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    const USoulsMeleeTraceComponent* Trace = Avatar ? Avatar->FindComponentByClass<USoulsMeleeTraceComponent>() : nullptr;
    const USoulsAttackDefinition* Definition = Trace ? Trace->GetAttackDefinition() : nullptr;
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Avatar);
    if (!Avatar || !Avatar->HasAuthority() || !Trace || Trace->IsAttackActive() || !Trace->GetWeapon() ||
        !Definition || !Definition->AttackMontage || !Definition->DamageEffectClass ||
        (Health && (Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)))
    {
        return false;
    }
    const USoulsCombatActionComponent* Combat = Avatar->FindComponentByClass<USoulsCombatActionComponent>();
    if (Combat && !Combat->CanStartLightAttack())
    {
        return false;
    }
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

// 服务器扣费并开启检测，拥有者客户端只播放确认后的动作。
void USoulsGameplayAbility_LightAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    USoulsMeleeTraceComponent* Trace = Avatar ? Avatar->FindComponentByClass<USoulsMeleeTraceComponent>() : nullptr;
    const USoulsAttackDefinition* Definition = Trace ? Trace->GetAttackDefinition() : nullptr;
    const bool bPlayerInput = TriggerEventData &&
        TriggerEventData->EventTag == FGameplayTag::RequestGameplayTag(TEXT("Event.Souls.LightAttack"));
    const int32 Stage = bPlayerInput && TriggerEventData->EventMagnitude == 2.0f ? 2 : 1;
    if (!Trace || !Definition || !Definition->AttackMontage || !ActorInfo->GetAnimInstance())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (Avatar->HasAuthority())
    {
        USoulsCombatActionComponent* Combat = Avatar->FindComponentByClass<USoulsCombatActionComponent>();
        if (!Definition->DamageEffectClass || (bPlayerInput && !Combat) ||
            !CommitAbility(Handle, ActorInfo, ActivationInfo) || !Trace->BeginAttack())
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
        ActiveTrace = Trace;
        if (Combat && !Combat->StartLightAttack(Stage))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
        ActiveCombat = Combat;
        HitDelegateHandle = Trace->OnHit.AddUObject(this, &ThisClass::HandleConfirmedHit);
    }

    constexpr float SecondStageRateScale = 1.2f;
    const float PlayRate = FMath::Max(Definition->PlayRate, 0.1f) * (Stage == 2 ? SecondStageRateScale : 1.0f);
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None,
        Definition->AttackMontage, PlayRate, NAME_None, true, 1.0f, 0.0f, true);
    if (!MontageTask)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
    MontageTask->ReadyForActivation();
}

// 即使动画通知未收到 NotifyEnd，结束能力仍立即停止真实伤害。
void USoulsGameplayAbility_LightAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (MontageTask)
    {
        MontageTask->OnCompleted.RemoveDynamic(this, &ThisClass::HandleMontageCompleted);
        MontageTask->OnInterrupted.RemoveDynamic(this, &ThisClass::HandleMontageInterrupted);
        MontageTask->OnCancelled.RemoveDynamic(this, &ThisClass::HandleMontageInterrupted);
    }
    if (USoulsMeleeTraceComponent* Trace = ActiveTrace.Get())
    {
        Trace->OnHit.Remove(HitDelegateHandle);
        Trace->EndAttack();
    }
    HitDelegateHandle.Reset();
    ActiveTrace.Reset();
    const TWeakObjectPtr<USoulsCombatActionComponent> Combat = ActiveCombat;
    ActiveCombat.Reset();
    MontageTask = nullptr;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
    if (USoulsCombatActionComponent* CombatActions = Combat.Get())
    {
        CombatActions->FinishLightAttack(bWasCancelled);
    }
}

// 每个目标使用独立 Context，伤害值由已有 LyraCombatSet 与 Execution 计算。
void USoulsGameplayAbility_LightAttack::HandleConfirmedHit(const FHitResult& Hit)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    USoulsMeleeTraceComponent* Trace = ActiveTrace.Get();
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hit.GetActor());
    const USoulsAttackDefinition* Definition = Trace ? Trace->GetAttackDefinition() : nullptr;
    if (!IsActive() || !Avatar || !Avatar->HasAuthority() || !Trace || !Trace->IsHitWindowEnabled() ||
        !SourceASC || !TargetASC || !Definition || !Definition->DamageEffectClass)
    {
        return;
    }
    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddInstigator(Avatar, Avatar);
    Context.AddSourceObject(Trace->GetWeapon());
    Context.AddHitResult(Hit, true);
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(Definition->DamageEffectClass, GetAbilityLevel(), Context);
    if (Spec.IsValid())
    {
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    }
}

// 完整动作结束后恢复普通移动并允许下一次输入。
void USoulsGameplayAbility_LightAttack::HandleMontageCompleted()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HasAuthority(&CurrentActivationInfo), false);
    }
}

// 播放失败、受击抢占和死亡都会走相同的取消清理路径。
void USoulsGameplayAbility_LightAttack::HandleMontageInterrupted()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HasAuthority(&CurrentActivationInfo), true);
    }
}
