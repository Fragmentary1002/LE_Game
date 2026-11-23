#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoulsAttackDefinition.generated.h"

class UAnimMontage;
class UGameplayEffect;

/** 配置单次近战动作、刀刃采样与起手转向，不保存运行时攻击状态。 */
UCLASS(BlueprintType)
class SOULSCORERUNTIME_API USoulsAttackDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Attack")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Attack")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Attack", meta = (ClampMin = "0.1"))
    float PlayRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Trace", meta = (ClampMin = "1"))
    float TraceRadius = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Trace", meta = (ClampMin = "2", ClampMax = "32"))
    int32 BladeSampleCount = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Trace", meta = (ClampMin = "1"))
    float MaxSubstepDistance = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Trace", meta = (ClampMin = "1", ClampMax = "90"))
    float MaxSubstepAngle = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Trace", meta = (ClampMin = "1", ClampMax = "64"))
    int32 MaxSubsteps = 16;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Turning", meta = (ClampMin = "0"))
    float TurnRate = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Turning", meta = (ClampMin = "0"))
    float TurnDuration = 0.12f;
};
