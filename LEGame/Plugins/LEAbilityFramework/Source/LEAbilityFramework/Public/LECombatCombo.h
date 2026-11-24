#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "LECombatInput.h"
#include "LECombatCombo.generated.h"

class ULEAbilityDefinition;

/** 定义一条从当前招式到目标招式的可配置派生。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLECombatComboTransition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTag TriggerInputIntentTag;

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTag TargetComboTag;

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTag RequiredWindow;

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTagContainer RequiredTags;

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTagContainer BlockedTags;

    UPROPERTY(EditAnywhere, Category = "Combo")
    int32 Priority = 0;
};

/** 以 Combo Tag 为行名，配置当前节点的技能和可用派生。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLECombatComboDefinitionRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTag ComboTag;

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTag AbilityTag;

    UPROPERTY(EditAnywhere, Category = "Combo")
    FGameplayTagContainer GrantedTags;

    UPROPERTY(EditAnywhere, Category = "Combo")
    TArray<FLECombatComboTransition> Transitions;
};

/** 以 Ability Tag 为行名，索引供 GAS 读取的技能定义资产。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLEAbilityRegistryRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Ability")
    FGameplayTag AbilityTag;

    UPROPERTY(EditAnywhere, Category = "Ability")
    TObjectPtr<ULEAbilityDefinition> AbilityDefinition;
};

/** 处理离散意图的缓存和连招出边匹配。 */
class LEABILITYFRAMEWORK_API FLECombatCombo
{
public:
    // 缓存最近的一个合法意图并按权威时间计算有效期。
    void Buffer(const FLECombatIntent& Intent, double Now, double BufferSeconds);

    // 查询是否有待消费的意图，包括等待清理的过期输入。
    bool HasBufferedInput() const;

    // 消费缓存并丢弃已超过有效期的意图。
    TOptional<FLECombatIntent> Consume(double Now);

    // 清理当前动作会话的待消费意图。
    void Reset();

    // 从匹配输入和窗口的出边中选择最高优先级的规则。
    static const FLECombatComboTransition* FindTransition(TConstArrayView<FLECombatComboTransition> Transitions,
        FGameplayTag IntentTag, const FGameplayTagContainer& OpenWindows,
        const FGameplayTagContainer& OwnedTags);

private:
    struct FBufferedIntent
    {
        FGameplayTag Tag;
        FVector Direction;
        double ExpiresAt;
    };

    TOptional<FBufferedIntent> BufferedIntent;
};
