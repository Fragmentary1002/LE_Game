#include "Combat/SoulsStaminaSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsStaminaSet)

// 让新创建的属性集从满精力开始。
USoulsStaminaSet::USoulsStaminaSet()
    : Stamina(100.0f), MaxStamina(100.0f)
{
}

// 精力用于拥有者的 HUD，并通过 GAS 通知属性订阅者。
void USoulsStaminaSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Stamina, COND_OwnerOnly, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxStamina, COND_OwnerOnly, REPNOTIFY_Always);
}

// 在修改基础值前统一夹取范围。
void USoulsStaminaSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
    Super::PreAttributeBaseChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

// 在计算当前值时沿用基础值的边界。
void USoulsStaminaSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

// 精力上限下调后主动修正已经超过上限的精力。
void USoulsStaminaSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);
    if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
    {
        if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent(); ASC && ASC->IsOwnerActorAuthoritative())
        {
            ASC->SetNumericAttributeBase(GetStaminaAttribute(), NewValue);
        }
    }
}

// 保证上限为正数，当前精力始终落在零与上限之间。
void USoulsStaminaSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
    if (Attribute == GetStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
    }
    else if (Attribute == GetMaxStaminaAttribute())
    {
        NewValue = FMath::Max(1.0f, NewValue);
    }
}

// 将当前精力的复制结果同步给 GAS 聚合器和监听者。
void USoulsStaminaSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Stamina, OldValue);
}

// 将最大精力的复制结果同步给 GAS 聚合器和监听者。
void USoulsStaminaSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxStamina, OldValue);
}
