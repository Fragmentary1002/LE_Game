#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoulsCombatEditorLibrary.generated.h"

class UAnimMontage;
class UAnimSequence;
class UBlueprint;

/** 将近战动画通知和伤害效果配置限制在编辑器生成工具中。 */
UCLASS()
class SOULSCOREEDITOR_API USoulsCombatEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 清理复制资源中的旧近战通知，建立单一全身动作和有效命中窗口。
    UFUNCTION(BlueprintCallable, Category = "Souls|Editor")
    static bool ConfigureAttackMontage(UAnimMontage* Montage, UAnimSequence* Sequence);

    // 为即时伤害蓝图配置现有 Lyra 伤害执行，为初始化蓝图配置基础攻击力。
    UFUNCTION(BlueprintCallable, Category = "Souls|Editor")
    static bool ConfigureCombatEffect(UBlueprint* Blueprint, bool bDamageExecution, float BaseDamage = 25.0f);

    // 检查攻击资源只包含一个有效窗口且底层动作没有遗留伤害通知。
    UFUNCTION(BlueprintPure, Category = "Souls|Editor")
    static bool ValidateAttackMontage(const UAnimMontage* Montage);
};
