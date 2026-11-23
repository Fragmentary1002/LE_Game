#include "Combat/Reactions/SoulsCombatReactionComponent.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Combat/Actions/SoulsCombatActionComponent.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsCombatReactionComponent)

// 绑定已就绪的能力系统，并确保同一个系统不会重复监听。
void USoulsCombatReactionComponent::InitializeWithAbilitySystem(ULyraAbilitySystemComponent* InASC)
{
    if (InASC == BoundASC.Get())
    {
        return;
    }
    UninitializeFromAbilitySystem();
    if (!InASC)
    {
        return;
    }
    BoundASC = InASC;
    BoundHealthSet = InASC->GetSet<ULyraHealthSet>();
    if (BoundHealthSet.IsValid())
    {
        BoundHealthSet->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
    }
}

// 清理旧生命集上的监听，避免换 Pawn 后仍向旧角色发送事件。
void USoulsCombatReactionComponent::UninitializeFromAbilitySystem()
{
    if (BoundHealthSet.IsValid())
    {
        BoundHealthSet->OnHealthChanged.RemoveAll(this);
    }
    BoundHealthSet.Reset();
    BoundASC.Reset();
}

// 只有能力系统和生命集同时存在才认为受击监听已接入。
bool USoulsCombatReactionComponent::IsInitialized() const
{
    return BoundASC.IsValid() && BoundHealthSet.IsValid();
}

// 优先跟随 Lyra 初始化委托，独立训练角色则直接查询自己的能力系统。
void USoulsCombatReactionComponent::BeginPlay()
{
    Super::BeginPlay();
    if (ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
    {
        Extension->OnAbilitySystemInitialized_RegisterAndCall(
            FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
        Extension->OnAbilitySystemUninitialized_Register(
            FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::UninitializeFromAbilitySystem));
    }
    if (!BoundASC.IsValid())
    {
        InitializeWithAbilitySystem(Cast<ULyraAbilitySystemComponent>(
            UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner())));
    }
}

// 所有退出路径都解除生命监听。
void USoulsCombatReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UninitializeFromAbilitySystem();
    Super::EndPlay(EndPlayReason);
}

// 接入 PlayerState ASC 与当前 Pawn 的映射。
void USoulsCombatReactionComponent::HandleAbilitySystemInitialized()
{
    if (const ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
    {
        InitializeWithAbilitySystem(Extension->GetLyraAbilitySystemComponent());
    }
}

// 掉血时清除翻滚及缓存，存活伤害携带原始命中上下文触发受击。
void USoulsCombatReactionComponent::HandleHealthChanged(AActor* Instigator, AActor* Causer,
    const FGameplayEffectSpec* Spec, float Magnitude, float OldHealth, float NewHealth)
{
    if (!GetOwner()->HasAuthority() || !BoundASC.IsValid() || NewHealth >= OldHealth)
    {
        return;
    }
    if (USoulsCombatActionComponent* Combat = GetOwner()->FindComponentByClass<USoulsCombatActionComponent>())
    {
        Combat->Interrupt();
    }
    if (!Spec || NewHealth <= 0.0f)
    {
        return;
    }
    FGameplayEventData Event;
    Event.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Souls.HitReact"));
    Event.Instigator = Instigator ? Instigator : Causer;
    Event.Target = BoundASC->GetAvatarActor();
    Event.ContextHandle = Spec->GetEffectContext();
    Event.EventMagnitude = OldHealth - NewHealth;
    BoundASC->HandleGameplayEvent(Event.EventTag, &Event);
}
