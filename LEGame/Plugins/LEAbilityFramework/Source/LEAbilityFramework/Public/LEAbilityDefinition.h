#pragma once

#include "Engine/DataAsset.h"
#include "LECombatTimelineTask.h"
#include "LEAbilityDefinition.generated.h"

class UAnimMontage;
class USkeletalMesh;

/** 配置招式动画的执行参数。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLEAbilityExecution
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Execution")
    TObjectPtr<UAnimMontage> Montage;

    UPROPERTY(EditAnywhere, Category = "Execution", meta = (ClampMin = "0.01"))
    float PlayRate = 1.0f;
};

/** 配置招式激活时的角色状态检查及与其他 GA 的交互。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLEAbilityInteraction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Interaction")
    FGameplayTagContainer ActivationRequiredTags;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    FGameplayTagContainer ActivationBlockedTags;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    FGameplayTagContainer ActivationOwnedTags;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    FGameplayTagContainer BlockAbilityTags;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    FGameplayTagContainer CancelAbilityTags;

    // 检查当前 ASC 标签是否满足资产的激活要求。
    bool CanActivate(const FGameplayTagContainer& OwnedTags) const;
};

/** 在同一个资产中编辑 GA、动作、技能窗口和交互规则。 */
UCLASS(BlueprintType)
class LEABILITYFRAMEWORK_API ULEAbilityDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    FGameplayTag AbilityTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    TSubclassOf<UGameplayAbility> AbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution")
    FLEAbilityExecution Execution;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline")
    TArray<FLECombatTimelineWindow> TimelineWindows;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline")
    TArray<FLECombatTimelinePoint> TimelinePoints;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    FLEAbilityInteraction Interaction;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Preview")
    TObjectPtr<USkeletalMesh> PreviewMesh;
#endif

#if WITH_EDITOR
    // 供资产编辑器和 UE 数据校验共用。
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
