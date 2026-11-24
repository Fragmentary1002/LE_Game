#include "Combat/Actions/SoulsCombatActionComponent.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Combat/Actions/SoulsStaminaSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameModes/SoulsGameMode.h"
#include "LEAbilityDefinition.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"
#include "Templates/UnrealTemplate.h"
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

    constexpr float RollDuration = 0.55f;
    constexpr float RollMovementDuration = 0.32f;
    constexpr float RollSpeed = 875.0f;
    constexpr float ImmunityStart = 0.08f;
    constexpr float ImmunityEnd = 0.30f;

    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Combo_Entry, "Combo.Souls.Entry");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Combo_LightOne, "Combo.Souls.Light.01");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Combo_LightTwo, "Combo.Souls.Light.02");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Combo_Roll, "Combo.Souls.Roll");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_LightOne, "Ability.Souls.LightAttack.01");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_LightTwo, "Ability.Souls.LightAttack.02");
    UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Roll, "Ability.Souls.Roll");

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
    ComboDefinitions = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
        TEXT("/SoulsCore/Combat/Combos/DT_ComboDefinitions.DT_ComboDefinitions")));
    AbilityRegistry = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
        TEXT("/SoulsCore/Combat/Combos/DT_AbilityRegistry.DT_AbilityRegistry")));
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
        LoadComboTables();
        if (ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
        {
            Extension->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemRemoved));
            Extension->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReady));
        }
    }
}

// 载入表数据并核对当前运行时所需的行类型。
void USoulsCombatActionComponent::LoadComboTables()
{
    const UDataTable* Definitions = ComboDefinitions.LoadSynchronous();
    const UDataTable* Registry = AbilityRegistry.LoadSynchronous();
    if (!Definitions || Definitions->GetRowStruct() != FLECombatComboDefinitionRow::StaticStruct() ||
        !Registry || Registry->GetRowStruct() != FLEAbilityRegistryRow::StaticStruct())
    {
        UE_LOG(LogTemp, Error, TEXT("Souls 连招表或技能注册表缺失，或者行类型不匹配。"));
        return;
    }

    LoadedComboDefinitions = Definitions;
    LoadedAbilityRegistry = Registry;
}

// 仅接受以 Combo Tag 命名并且标签字段一致的节点行。
const FLECombatComboDefinitionRow* USoulsCombatActionComponent::FindComboNode(FGameplayTag ComboTag) const
{
    const FLECombatComboDefinitionRow* Node = LoadedComboDefinitions ? LoadedComboDefinitions->FindRow<FLECombatComboDefinitionRow>(
        ComboTag.GetTagName(), TEXT("Souls Combo"), false) : nullptr;
    return Node && Node->ComboTag == ComboTag ? Node : nullptr;
}

// 按本次节点的配置增加 ASC 标签计数。
void USoulsCombatActionComponent::EnterComboNode(FGameplayTag ComboTag)
{
    ActiveComboTag = ComboTag;
    const FLECombatComboDefinitionRow* Node = FindComboNode(ComboTag);
    UAbilitySystemComponent* ASC = BoundASC.Get();
    if (Node && ASC)
    {
        for (const FGameplayTag& Tag : Node->GrantedTags)
        {
            ActiveGrantedTags.AddTag(Tag);
            ASC->AddLooseGameplayTag(Tag);
            if (ActiveComboTag != ComboTag)
            {
                return;
            }
        }
    }
}

// 先解除节点状态，再收回自己添加的标签计数。
FGameplayTag USoulsCombatActionComponent::LeaveComboNode()
{
    const FGameplayTag FinishedComboTag = ActiveComboTag;
    ActiveComboTag = FGameplayTag();
    const FGameplayTagContainer GrantedTags = MoveTemp(ActiveGrantedTags);
    ActiveGrantedTags.Reset();
    if (UAbilitySystemComponent* ASC = BoundASC.Get())
    {
        ASC->RemoveLooseGameplayTags(GrantedTags);
    }
    return FinishedComboTag;
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
        LeaveComboNode();
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
    const FGameplayTag IntentTag = FGameplayTag::RequestGameplayTag(Input == ESoulsCombatInput::Roll
        ? TEXT("InputIntent.Souls.Roll") : TEXT("InputIntent.Souls.LightAttack"));
    const FLECombatIntent Intent{IntentTag, Direction};
    if (ActiveComboTag.IsValid() || bRolling || DrainTimer.IsValid())
    {
        Combo.Buffer(Intent, GetWorld()->GetTimeSeconds(), InputBufferSeconds);
        BufferedComboWindows = OpenComboWindows;
    }
    else
    {
        DispatchInput(Intent, TAG_Combo_Entry, OpenComboWindows);
    }
}

// 由输入意图定位目标 Combo，再通过目标的 Ability Tag 解析技能定义。
void USoulsCombatActionComponent::DispatchInput(const FLECombatIntent& Intent, FGameplayTag SourceComboTag,
    const FGameplayTagContainer& Windows)
{
    const FLECombatComboDefinitionRow* Source = FindComboNode(SourceComboTag);
    if (!Source || !LoadedAbilityRegistry)
    {
        return;
    }

    UAbilitySystemComponent* ASC = BoundASC.Get();
    FGameplayTagContainer OwnedTags;
    ASC->GetOwnedGameplayTags(OwnedTags);
    const FLECombatComboTransition* Transition = FLECombatCombo::FindTransition(
        Source->Transitions, Intent.Tag, Windows, OwnedTags);
    const FLECombatComboDefinitionRow* Target = Transition ? FindComboNode(Transition->TargetComboTag) : nullptr;
    if (!Target || !Target->AbilityTag.IsValid())
    {
        return;
    }
    if (Target->AbilityTag == TAG_Ability_Roll)
    {
        StartRoll(Intent.Direction, Target->ComboTag);
        return;
    }

    const FLEAbilityRegistryRow* Ability = LoadedAbilityRegistry->FindRow<FLEAbilityRegistryRow>(
        Target->AbilityTag.GetTagName(), TEXT("Souls Ability Registry"));
    const ULEAbilityDefinition* Definition = Ability ? Ability->AbilityDefinition.Get() : nullptr;
    if (!Definition || Ability->AbilityTag != Target->AbilityTag || Definition->AbilityTag != Ability->AbilityTag ||
        !Definition->AbilityClass || !Definition->Interaction.CanActivate(OwnedTags))
    {
        return;
    }

    if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Definition->AbilityClass))
    {
        TGuardValue<FGameplayTag> PendingNodeGuard(PendingComboTag, Target->ComboTag);
        FGameplayEventData Event;
        Event.EventTag = Ability->AbilityTag;
        Event.Instigator = GetOwner();
        Event.Target = GetOwner();
        Event.OptionalObject = Definition;
        ASC->TriggerAbilityFromGameplayEvent(Spec->Handle, ASC->AbilityActorInfo.Get(), Event.EventTag, &Event, *ASC);
    }
}

// 记录合法的开窗输入，使接续能在旧 GA 完成后安全启动。
void USoulsCombatActionComponent::SetComboWindow(FGameplayTag Window, bool bOpen)
{
    if (bOpen)
    {
        OpenComboWindows.AddTag(Window);
        if (Combo.HasBufferedInput())
        {
            BufferedComboWindows.AddTag(Window);
        }
    }
    else
    {
        OpenComboWindows.RemoveTag(Window);
    }
}

// 等待旧 Montage 的完成回调退出后再尝试激活下一招。
void USoulsCombatActionComponent::ScheduleDrain(FGameplayTag FinishedComboTag)
{
    if (Combo.HasBufferedInput() && CanAct())
    {
        DrainTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(this, &ThisClass::DrainInput, FinishedComboTag));
    }
}

// 先取出并清空缓存，保证拒绝起招的意图也只消费一次。
void USoulsCombatActionComponent::DrainInput(FGameplayTag FinishedComboTag)
{
    DrainTimer.Invalidate();
    const TOptional<FLECombatIntent> Intent = Combo.Consume(GetWorld()->GetTimeSeconds());
    const FGameplayTagContainer Windows = MoveTemp(BufferedComboWindows);
    BufferedComboWindows.Reset();
    if (Intent.IsSet() && CanAct() && !ActiveComboTag.IsValid() && !bRolling)
    {
        DispatchInput(Intent.GetValue(), FinishedComboTag, Windows);
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
    return CanAct() && !bRolling && !ActiveComboTag.IsValid() &&
        BoundASC->GetNumericAttribute(USoulsStaminaSet::GetStaminaAttribute()) >= LightCost;
}

// 每个轻击段独立扣费并登记，缓存操作不扣费。
bool USoulsCombatActionComponent::StartLightAttack(int32 Stage)
{
    if (!CanStartLightAttack() || !SpendStamina(LightCost))
    {
        return false;
    }
    const FGameplayTag DirectComboTag = Stage == 2 ? TAG_Combo_LightTwo.GetTag() : TAG_Combo_LightOne.GetTag();
    EnterComboNode(PendingComboTag.IsValid() ? PendingComboTag : DirectComboTag);
    return true;
}

// 只有正常完成的轻击可以消费缓存进入下一招。
void USoulsCombatActionComponent::FinishLightAttack(bool bCancelled)
{
    if (!ActiveComboTag.IsValid() || bRolling)
    {
        return;
    }
    OpenComboWindows.Reset();
    const FGameplayTag FinishedComboTag = LeaveComboNode();
    if (bCancelled)
    {
        Interrupt();
    }
    else
    {
        ScheduleDrain(FinishedComboTag);
    }
}

// 取消缓存和翻滚，轻击资源仍由轻击 GA 的结束路径释放。
void USoulsCombatActionComponent::Interrupt()
{
    Combo.Reset();
    OpenComboWindows.Reset();
    BufferedComboWindows.Reset();
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
void USoulsCombatActionComponent::StartRoll(const FVector& Direction, FGameplayTag ComboTag)
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
    PlayRouteRollMontage(*Character);
    Character->ForceNetUpdate();
    EnterComboNode(ComboTag);
}

// 闪避动作只负责表现，实际位移仍由当前 Root Motion Source 驱动。
void USoulsCombatActionComponent::PlayRouteRollMontage(ACharacter& Character)
{
    const ASoulsGameMode* Mode = GetWorld()->GetAuthGameMode<ASoulsGameMode>();
    UAnimMontage* Montage = Mode ? Mode->GetRouteDodgeMontage() : nullptr;
    UAnimInstance* Anim = Character.GetMesh()->GetAnimInstance();
    if (Montage && Anim && Anim->Montage_Play(Montage, Montage->GetPlayLength() / RollDuration) > 0.0f)
    {
        if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage))
        {
            Instance->PushDisableRootMotion();
        }
        PlayingRollMontage = Montage;
    }
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
        if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance(); Anim && PlayingRollMontage.IsValid())
        {
            Anim->Montage_Stop(0.08f, PlayingRollMontage.Get());
        }
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
    PlayingRollMontage.Reset();
    const FGameplayTag FinishedComboTag = LeaveComboNode();
    if (!bCancelled)
    {
        ScheduleDrain(FinishedComboTag);
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
    if (ActiveComboTag.IsValid() || ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact"))))
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
