#include "Combat/SoulsCombatActionComponent.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Combat/SoulsStaminaSet.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Targeting/SoulsLockOnComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsCombatActionComponent)

namespace
{
    constexpr float LightCost = 15.0f;
    constexpr float RollCost = 25.0f;
    constexpr float RecoveryDelay = 0.8f;
    constexpr float RecoveryPerSecond = 20.0f;
    constexpr float BufferSeconds = 0.18f;
    constexpr float RollDuration = 0.55f;
    constexpr float RollMovementDuration = 0.32f;
    constexpr float RollSpeed = 875.0f;
    constexpr float ImmunityStart = 0.08f;
    constexpr float ImmunityEnd = 0.30f;

    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_LightAttack, "Event.Souls.LightAttack");
}

// 用现有 Lyra 免疫标签处理正常伤害，效果独立过期。
USoulsRollImmunityEffect::USoulsRollImmunityEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(ImmunityEnd - ImmunityStart));
    FInheritedTagContainer GrantedTags;
    GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Gameplay.DamageImmunity")));
    UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("RollImmunityTags"));
    GEComponents.Add(TargetTags);
    TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}

// 只在有翻滚或精力缺口时启用服务器 Tick。
USoulsCombatActionComponent::USoulsCombatActionComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// 将翻滚控制状态同步到拥有者。
void USoulsCombatActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ThisClass, bRolling, COND_OwnerOnly);
}

// 绑定当前 Pawn 的 ASC 生命周期。
void USoulsCombatActionComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority())
    {
        if (ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
        {
            Extension->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemRemoved));
            Extension->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReady));
        }
    }
}

// 退出前释放已预约的回调、免疫和移动源。
void USoulsCombatActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    HandleAbilitySystemRemoved();
    Super::EndPlay(EndPlayReason);
}

// 在 PlayerState ASC 上只创建一份精力属性集，换 Pawn 时回满。
void USoulsCombatActionComponent::HandleAbilitySystemReady()
{
    const ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
    UAbilitySystemComponent* ASC = Extension ? Extension->GetLyraAbilitySystemComponent() : nullptr;
    if (!IsValid(ASC) || ASC->GetAvatarActor() != GetOwner() || BoundASC.Get() == ASC)
    {
        return;
    }
    BoundASC = ASC;
    if (!ASC->GetSet<USoulsStaminaSet>())
    {
        ASC->AddAttributeSetSubobject(NewObject<USoulsStaminaSet>(ASC->GetOwner()));
    }
    ASC->SetNumericAttributeBase(USoulsStaminaSet::GetStaminaAttribute(), ASC->GetNumericAttribute(USoulsStaminaSet::GetMaxStaminaAttribute()));
}

// 停止恢复精力并与旧 Avatar 的动作彻底解绑。
void USoulsCombatActionComponent::HandleAbilitySystemRemoved()
{
    if (GetOwner()->HasAuthority())
    {
        Interrupt();
        ActiveLightStage = 0;
        BoundASC.Reset();
        SetComponentTickEnabled(false);
    }
}

// 本地权威沿同一入口处理，远端拥有者通过 Pawn 组件 RPC 提交。
void USoulsCombatActionComponent::SubmitInput(ESoulsCombatInput Input, const FVector& Direction)
{
    const APawn* Pawn = GetPawn<APawn>();
    if (!Pawn || !Pawn->IsLocallyControlled() || Direction.ContainsNaN())
    {
        return;
    }
    const FVector FlatDirection = Direction.GetSafeNormal2D();
    if (Pawn->HasAuthority())
    {
        AcceptInput(Input, FlatDirection);
    }
    else
    {
        ServerSubmitInput(Input, FlatDirection);
    }
}

// 依据服务器收包时刻计算有效期，只接受有限的平面方向。
void USoulsCombatActionComponent::ServerSubmitInput_Implementation(ESoulsCombatInput Input, FVector_NetQuantizeNormal Direction)
{
    if (!Direction.ContainsNaN())
    {
        AcceptInput(Input, Direction.GetSafeNormal2D());
    }
}

// 动作中只保留最后一次按下，空闲时立即尝试起动。
void USoulsCombatActionComponent::AcceptInput(ESoulsCombatInput Input, const FVector& Direction)
{
    if ((Input != ESoulsCombatInput::LightAttack && Input != ESoulsCombatInput::Roll) || !CanAct())
    {
        return;
    }
    const FBufferedInput Intent{Input, Direction, GetWorld()->GetTimeSeconds() + BufferSeconds};
    if (ActiveLightStage != 0 || bRolling || DrainTimer.IsValid())
    {
        BufferedInput = Intent;
    }
    else
    {
        DispatchInput(Intent, 0, false);
    }
}

// 由服务器按本次接续来源确定轻击段号或翻滚。
void USoulsCombatActionComponent::DispatchInput(const FBufferedInput& Intent, int32 FinishedLightStage, bool bFinishedRoll)
{
    if (Intent.Input == ESoulsCombatInput::Roll)
    {
        if (!bFinishedRoll)
        {
            StartRoll(Intent.Direction);
        }
        return;
    }
    if (FinishedLightStage != 2)
    {
        FGameplayEventData Event;
        Event.EventTag = TAG_Event_LightAttack;
        Event.Instigator = GetOwner();
        Event.Target = GetOwner();
        Event.EventMagnitude = FinishedLightStage == 1 ? 2.0f : 1.0f;
        BoundASC->HandleGameplayEvent(Event.EventTag, &Event);
    }
}

// 等待旧 Montage 的完成回调退出后再尝试激活下一招。
void USoulsCombatActionComponent::ScheduleDrain(int32 FinishedLightStage, bool bFinishedRoll)
{
    if (BufferedInput.IsSet() && CanAct())
    {
        DrainTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(this, &ThisClass::DrainInput, FinishedLightStage, bFinishedRoll));
    }
}

// 先取出并清空缓存，保证拒绝起招的意图也只消费一次。
void USoulsCombatActionComponent::DrainInput(int32 FinishedLightStage, bool bFinishedRoll)
{
    DrainTimer.Invalidate();
    const TOptional<FBufferedInput> Intent = MoveTemp(BufferedInput);
    BufferedInput.Reset();
    if (Intent.IsSet() && CanAct() && ActiveLightStage == 0 && !bRolling && GetWorld()->GetTimeSeconds() < Intent->ExpiresAt)
    {
        DispatchInput(Intent.GetValue(), FinishedLightStage, bFinishedRoll);
    }
}

// 死亡、受击、解绑和输入屏蔽期间拒绝战斗操作。
bool USoulsCombatActionComponent::CanAct() const
{
    const APawn* Pawn = GetPawn<APawn>();
    const UAbilitySystemComponent* ASC = BoundASC.Get();
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
    return Pawn && Pawn->HasAuthority() && Pawn->GetController() && GetWorld() && ASC && ASC->GetAvatarActor() == Pawn &&
        Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f &&
        !ASC->HasMatchingGameplayTag(TAG_Gameplay_AbilityInputBlocked) &&
        !ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact")));
}

// 在 GAS 激活检查中提前拒绝精力不足或正在翻滚的轻击。
bool USoulsCombatActionComponent::CanStartLightAttack() const
{
    return CanAct() && !bRolling && ActiveLightStage == 0 &&
        BoundASC->GetNumericAttribute(USoulsStaminaSet::GetStaminaAttribute()) >= LightCost;
}

// 每个轻击段独立扣费并登记，缓存操作不扣费。
bool USoulsCombatActionComponent::StartLightAttack(int32 Stage)
{
    if (!CanStartLightAttack() || !SpendStamina(LightCost))
    {
        return false;
    }
    ActiveLightStage = Stage;
    return true;
}

// 只有正常完成的轻击可以消费缓存进入下一招。
void USoulsCombatActionComponent::FinishLightAttack(bool bCancelled)
{
    if (ActiveLightStage == 0)
    {
        return;
    }
    const int32 FinishedStage = ActiveLightStage;
    ActiveLightStage = 0;
    if (bCancelled)
    {
        Interrupt();
    }
    else
    {
        ScheduleDrain(FinishedStage, false);
    }
}

// 取消缓存和翻滚，轻击资源仍由轻击 GA 的结束路径释放。
void USoulsCombatActionComponent::Interrupt()
{
    BufferedInput.Reset();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DrainTimer);
    }
    FinishRoll(true);
}

// 检查实际可用精力，成功支出后延后恢复。
bool USoulsCombatActionComponent::SpendStamina(float Cost)
{
    UAbilitySystemComponent* ASC = BoundASC.Get();
    const FGameplayAttribute Attribute = USoulsStaminaSet::GetStaminaAttribute();
    if (!ASC || ASC->GetNumericAttribute(Attribute) < Cost)
    {
        return false;
    }
    ASC->ApplyModToAttribute(Attribute, EGameplayModOp::Additive, -Cost);
    LastSpendAt = GetWorld()->GetTimeSeconds();
    SetComponentTickEnabled(true);
    return true;
}

// 施加可碰撞的短距离位移，并把方向固定在动作起动的一刻。
void USoulsCombatActionComponent::StartRoll(const FVector& Direction)
{
    ACharacter* Character = GetPawn<ACharacter>();
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement || !Movement->IsMovingOnGround() || !SpendStamina(RollCost))
    {
        return;
    }
    const FVector RollDirection = Direction.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : Direction;
    RollStartedAt = GetWorld()->GetTimeSeconds();
    bImmunityWindowEntered = false;
    bRolling = true;
    OnRep_Rolling();
    Character->SetActorRotation(RollDirection.Rotation());
    Character->ConsumeMovementInputVector();
    Movement->StopMovementImmediately();
    PreRollMaxWalkSpeed = Movement->MaxWalkSpeed;
    Movement->MaxWalkSpeed = 0.0f;

    const TSharedPtr<FRootMotionSource_ConstantForce> Motion = MakeShared<FRootMotionSource_ConstantForce>();
    Motion->InstanceName = TEXT("SoulsRoll");
    Motion->Priority = 500;
    Motion->AccumulateMode = ERootMotionAccumulateMode::Override;
    Motion->Force = RollDirection * RollSpeed;
    Motion->Duration = RollMovementDuration;
    Motion->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
    Motion->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
    RollMotionId = Movement->ApplyRootMotionSource(Motion);
    Character->ForceNetUpdate();
}

// 按句柄移除本次翻滚授予的效果与位移源。
void USoulsCombatActionComponent::FinishRoll(bool bCancelled)
{
    if (!bRolling || !GetOwner()->HasAuthority())
    {
        return;
    }
    bRolling = false;
    if (ImmunityHandle.IsValid())
    {
        const FActiveGameplayEffectHandle Handle = ImmunityHandle;
        ImmunityHandle.Invalidate();
        if (UAbilitySystemComponent* ASC = BoundASC.Get())
        {
            ASC->RemoveActiveGameplayEffect(Handle);
        }
    }
    if (ACharacter* Character = GetPawn<ACharacter>())
    {
        UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        Movement->MaxWalkSpeed = PreRollMaxWalkSpeed;
        if (RollMotionId != 0)
        {
            Movement->RemoveRootMotionSourceByID(RollMotionId);
            RollMotionId = 0;
        }
        OnRep_Rolling();
        Character->ForceNetUpdate();
    }
    if (!bCancelled)
    {
        ScheduleDrain(0, true);
    }
}

// 翻滚中保持起动朝向，存活时根据当前锁定情况恢复。
void USoulsCombatActionComponent::OnRep_Rolling()
{
    ACharacter* Character = GetPawn<ACharacter>();
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Character);
    if (!Character || (!bRolling && Health && (Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)))
    {
        return;
    }
    const USoulsLockOnComponent* LockOn = Character->FindComponentByClass<USoulsLockOnComponent>();
    const bool bLocked = LockOn && LockOn->GetTargetActor();
    Character->bUseControllerRotationYaw = false;
    Character->GetCharacterMovement()->bOrientRotationToMovement = !bRolling && !bLocked;
    Character->GetCharacterMovement()->bUseControllerDesiredRotation = !bRolling && bLocked;
}

// 在权威侧推进翻滚计时，闲置且超过恢复延迟时补充精力。
void USoulsCombatActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    APawn* Pawn = GetPawn<APawn>();
    UAbilitySystemComponent* ASC = BoundASC.Get();
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
    if (!ASC || !Pawn || !Pawn->HasAuthority() || ASC->GetAvatarActor() != Pawn || !Health ||
        Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)
    {
        Interrupt();
        SetComponentTickEnabled(false);
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (bRolling)
    {
        const double Elapsed = Now - RollStartedAt;
        if (!Pawn->GetMovementComponent()->IsMovingOnGround())
        {
            Interrupt();
        }
        else if (Elapsed >= RollDuration)
        {
            FinishRoll(false);
        }
        else if (!bImmunityWindowEntered && Elapsed >= ImmunityStart && Elapsed < ImmunityEnd)
        {
            bImmunityWindowEntered = true;
            FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(USoulsRollImmunityEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
            if (Spec.IsValid())
            {
                Spec.Data->SetDuration(static_cast<float>(ImmunityEnd - Elapsed), true);
                ImmunityHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
            }
        }
        if (ImmunityHandle.IsValid() && Elapsed >= ImmunityEnd)
        {
            ASC->RemoveActiveGameplayEffect(ImmunityHandle);
            ImmunityHandle.Invalidate();
        }
        return;
    }
    if (ActiveLightStage != 0 || ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact"))))
    {
        return;
    }
    const FGameplayAttribute Stamina = USoulsStaminaSet::GetStaminaAttribute();
    const float MaxStamina = ASC->GetNumericAttribute(USoulsStaminaSet::GetMaxStaminaAttribute());
    const float RecoverSeconds = static_cast<float>(FMath::Clamp(Now - LastSpendAt - RecoveryDelay, 0.0, static_cast<double>(DeltaTime)));
    if (RecoverSeconds > 0.0f)
    {
        ASC->ApplyModToAttribute(Stamina, EGameplayModOp::Additive, RecoveryPerSecond * RecoverSeconds);
    }
    if (ASC->GetNumericAttribute(Stamina) >= MaxStamina)
    {
        SetComponentTickEnabled(false);
    }
}
