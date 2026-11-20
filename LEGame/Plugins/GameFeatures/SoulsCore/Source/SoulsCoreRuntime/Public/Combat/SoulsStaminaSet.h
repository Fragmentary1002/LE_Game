#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "SoulsStaminaSet.generated.h"

/** 挂载到玩家 ASC 的当前精力和最大精力。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsStaminaSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    // 将初始精力设置为满值。
    USoulsStaminaSet();

    // 注册两个属性的 GAS 复制通知。
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 约束基础精力的写入范围。
    virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

    // 约束应用修饰后的精力范围。
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    // 最大精力降低时同步收紧当前值。
    virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(USoulsStaminaSet, Stamina)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(Stamina)
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(USoulsStaminaSet, MaxStamina)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxStamina)

private:
    // 将属性值夹在精力系统支持的范围内。
    void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

    // 向客户端 ASC 通知当前精力变化。
    UFUNCTION()
    void OnRep_Stamina(const FGameplayAttributeData& OldValue);

    // 向客户端 ASC 通知精力上限变化。
    UFUNCTION()
    void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

    UPROPERTY(ReplicatedUsing = OnRep_Stamina)
    FGameplayAttributeData Stamina;

    UPROPERTY(ReplicatedUsing = OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
};
