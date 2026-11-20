#include "Combat/SoulsGameplayAbility_HitReact.h"

#include "AI/SoulsEnemyController.h"
#include "GameFramework/Pawn.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/LyraHealthComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsGameplayAbility_HitReact)

// 硬直由生命结算事件触发，并在激活时取消轻攻击。
USoulsGameplayAbility_HitReact::USoulsGameplayAbility_HitReact(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ActivationGroup = ELyraAbilityActivationGroup::Independent;
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Souls.HitReact")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Death")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Souls.LightAttack")));
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        FAbilityTriggerData Trigger;
        Trigger.TriggerTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Souls.HitReact"));
        Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
        AbilityTriggers.Add(Trigger);
    }
}

// 保留 Boss 当前出招，其他角色继续沿用原有的受击取消规则。
bool USoulsGameplayAbility_HitReact::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
    const ASoulsEnemyController* Controller = Pawn ? Cast<ASoulsEnemyController>(Pawn->GetController()) : nullptr;
    return (!Controller || !Controller->HasBossAttackArmor()) &&
        Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

// 在硬直期间停下移动，动画缺失或播放失败时也能立即收尾。
void USoulsGameplayAbility_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    UAnimMontage* Montage = SelectMontage(TriggerEventData);
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character || !Montage || !ActorInfo->GetAnimInstance())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    LockedMovement = Character->GetCharacterMovement();
    PreviousMovementMode = LockedMovement->MovementMode;
    PreviousCustomMode = LockedMovement->CustomMovementMode;
    LockedMovement->StopMovementImmediately();
    LockedMovement->DisableMovement();
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,
        NAME_None, Montage, 1.0f, NAME_None, true);
    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageEnded);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageEnded);
    MontageTask->ReadyForActivation();
}

// 取消硬直时不重新启用已进入死亡流程的角色。
void USoulsGameplayAbility_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle,
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
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo());
    if (LockedMovement.IsValid() && (!Health || (!Health->IsDeadOrDying() && Health->GetHealth() > 0.0f)))
    {
        LockedMovement->SetMovementMode(PreviousMovementMode, PreviousCustomMode);
    }
    LockedMovement.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 以攻击者位置优先判断来袭方向，缺失来源时退回命中轨迹起点。
UAnimMontage* USoulsGameplayAbility_HitReact::SelectMontage(const FGameplayEventData* EventData) const
{
    const AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar || !EventData)
    {
        return FrontMontage;
    }
    FVector SourceLocation = Avatar->GetActorLocation() + Avatar->GetActorForwardVector();
    if (const AActor* Source = EventData->Instigator.Get())
    {
        SourceLocation = Source->GetActorLocation();
    }
    else if (const FHitResult* Hit = EventData->ContextHandle.GetHitResult())
    {
        SourceLocation = Hit->TraceStart;
    }
    const FVector LocalDirection = Avatar->GetActorTransform().InverseTransformVectorNoScale(
        SourceLocation - Avatar->GetActorLocation());
    UAnimMontage* Selected = nullptr;
    if (FMath::Abs(LocalDirection.X) >= FMath::Abs(LocalDirection.Y))
    {
        Selected = LocalDirection.X >= 0.0f ? FrontMontage.Get() : BackMontage.Get();
    }
    else
    {
        Selected = LocalDirection.Y >= 0.0f ? RightMontage.Get() : LeftMontage.Get();
    }
    return Selected ? Selected : FrontMontage.Get();
}

// 所有动画回调汇合到能力结束，避免遗留硬直标签。
void USoulsGameplayAbility_HitReact::HandleMontageEnded()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}
