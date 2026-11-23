#include "Combat/Melee/SoulsGameplayAbility_LightAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/Melee/AnimNotifyState_SoulsMeleeWindow.h"
#include "LECombatTimelineTask.h"
#include "NativeGameplayTags.h"
#include "Enemies/SoulsEnemy.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/Melee/SoulsAttackDefinition.h"
#include "Combat/Actions/SoulsCombatActionComponent.h"
#include "Combat/Melee/SoulsMeleeTraceComponent.h"
#include "Combat/Melee/SoulsMeleeWeapon.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsGameplayAbility_LightAttack)

namespace
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Window_Souls_MeleeHit, "Window.Souls.MeleeHit");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Window_Souls_ComboLight, "Window.Souls.Combo.Light");

    // 优先使用定义中的命中窗口，旧资产可使用 Montage 轨道上的时间。
    bool PopulatePlayerTimeline(const USoulsAttackDefinition& Definition, int32 Stage, TArray<FLECombatTimelineWindow>& Windows)
    {
        Windows = Definition.TimelineWindows;
        if (!Windows.ContainsByPredicate([](const FLECombatTimelineWindow& Window)
        {
            return Window.Tag == TAG_Window_Souls_MeleeHit && Window.IsValid();
        }))
        {
            for (const FAnimNotifyEvent& Notify : Definition.AttackMontage->Notifies)
            {
                if (Cast<UAnimNotifyState_SoulsMeleeWindow>(Notify.NotifyStateClass.Get()))
                {
                    FLECombatTimelineWindow Window{TAG_Window_Souls_MeleeHit,
                        FMath::Max(0.0f, Notify.GetTriggerTime()), Notify.GetEndTriggerTime()};
                    if (Window.IsValid())
                    {
                        Windows.Add(Window);
                    }
                }
            }
        }
        if (Stage == 1 && !Windows.ContainsByPredicate([](const FLECombatTimelineWindow& Window)
        {
            return Window.Tag == TAG_Window_Souls_ComboLight && Window.IsValid();
        }))
        {
            // 旧资产默认在最后 0.25 秒素材时间内接收轻击派生。
            const float End = Definition.AttackMontage->GetPlayLength();
            Windows.Add({TAG_Window_Souls_ComboLight, FMath::Max(0.0f, End - 0.25f), End});
        }
        return Windows.ContainsByPredicate([](const FLECombatTimelineWindow& Window)
        {
            return Window.Tag == TAG_Window_Souls_MeleeHit && Window.IsValid();
        });
    }
}

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
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        FAbilityTriggerData Trigger;
        Trigger.TriggerTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Souls.EnemyAttack"));
        Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
        AbilityTriggers.Add(Trigger);
    }
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
    float LungeDistance = 0.0f;
    if (TriggerEventData && !bPlayerInput)
    {
        if (!Cast<ASoulsEnemy>(Avatar) || TriggerEventData->Instigator.Get() != Avatar ||
            TriggerEventData->Target.Get() != Avatar ||
            TriggerEventData->EventTag != FGameplayTag::RequestGameplayTag(TEXT("Event.Souls.EnemyAttack")))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
        Definition = Cast<USoulsAttackDefinition>(TriggerEventData->OptionalObject.Get());
        LungeDistance = TriggerEventData->EventMagnitude;
    }
    if (!Trace || !Definition || !Definition->AttackMontage || !ActorInfo->GetAnimInstance())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    bool bUseAbilityHitWindow = false;
    TArray<FLECombatTimelineWindow> TimelineWindows;
    if (Avatar->HasAuthority())
    {
        USoulsCombatActionComponent* Combat = Avatar->FindComponentByClass<USoulsCombatActionComponent>();
        if (Combat)
        {
            bUseAbilityHitWindow = PopulatePlayerTimeline(*Definition, Stage, TimelineWindows);
        }
        if (!Definition->DamageEffectClass || (bPlayerInput && !Combat) ||
            !CommitAbility(Handle, ActorInfo, ActivationInfo) || !Trace->BeginAttack(Definition, LungeDistance, bUseAbilityHitWindow))
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
    if (IsActive() && ActiveCombat.IsValid() && (!TimelineWindows.IsEmpty() || !Definition->TimelinePoints.IsEmpty()))
    {
        const FAnimMontageInstance* Instance = ActorInfo->GetAnimInstance()->GetActiveInstanceForMontage(Definition->AttackMontage);
        const float EffectiveRate = (Instance ? Instance->GetPlayRate() : PlayRate) * Definition->AttackMontage->RateScale;
        TimelineTask = ULECombatTimelineTask::Create(this, TimelineWindows, Definition->TimelinePoints, EffectiveRate);
        TimelineTask->OnWindowChanged.AddUObject(this, &ThisClass::HandleTimelineWindow);
        TimelineTask->OnPointReached.AddUObject(this, &ThisClass::HandleTimelinePoint);
        TimelineTask->ReadyForActivation();
    }
}

// 即使动画通知未收到 NotifyEnd，结束能力仍立即停止真实伤害。
void USoulsGameplayAbility_LightAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (TimelineTask)
    {
        ULECombatTimelineTask* Task = TimelineTask.Get();
        TimelineTask = nullptr;
        if (!Task->IsFinished())
        {
            Task->EndTask();
        }
    }
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

// 由本次 GA 选择窗口接收方，同一命中窗口只交给刀刃检测。
void USoulsGameplayAbility_LightAttack::HandleTimelineWindow(FGameplayTag Tag, bool bOpen)
{
    if (Tag == TAG_Window_Souls_MeleeHit)
    {
        if (USoulsMeleeTraceComponent* Trace = ActiveTrace.Get())
        {
            Trace->SetHitWindowEnabled(bOpen);
        }
    }
    else if (USoulsCombatActionComponent* Combat = ActiveCombat.Get())
    {
        Combat->SetComboWindow(Tag, bOpen);
    }
}

// 使用 ASC 派发一次已配置的 Gameplay Event。
void USoulsGameplayAbility_LightAttack::HandleTimelinePoint(FGameplayTag Tag)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        FGameplayEventData Event;
        Event.EventTag = Tag;
        Event.Instigator = GetAvatarActorFromActorInfo();
        Event.Target = Event.Instigator;
        ASC->HandleGameplayEvent(Tag, &Event);
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
