#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SoulsBossProfile.generated.h"

class USoulsAttackDefinition;
class UAIPawnData;

USTRUCT()
struct FSoulsBossAttackOption
{
    GENERATED_BODY()

    UPROPERTY()
    FName AttackId;

    UPROPERTY()
    TSoftObjectPtr<USoulsAttackDefinition> Definition;

    UPROPERTY()
    float MinRange = 0.0f;

    UPROPERTY()
    float MaxRange = 0.0f;

    UPROPERTY()
    int32 MinPhase = 1;

    UPROPERTY()
    float PostAttackPause = 0.0f;

    UPROPERTY()
    float LungeDistance = 0.0f;
};

/** 配置重剑守卫专用的招式和阶段，按出生资产匹配，不影响普通剑兵。 */
UCLASS(Config = Game, DefaultConfig)
class SOULSCORERUNTIME_API USoulsBossProfile : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config)
    TSoftObjectPtr<UAIPawnData> PawnData;

    UPROPERTY(Config)
    TArray<FSoulsBossAttackOption> AttackOptions;

    UPROPERTY(Config)
    float PhaseTwoHealthFraction = 0.5f;

    UPROPERTY(Config)
    float PhaseChangePause = 0.45f;
};
