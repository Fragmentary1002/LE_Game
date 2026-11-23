#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerState.h"
#include "SoulsPlayerState.generated.h"

/** 为魂类玩法提供局内玩家状态扩展点。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsPlayerState : public ALyraPlayerState
{
    GENERATED_BODY()

public:
    // 沿用 Lyra 的玩家状态与能力系统初始化。
    ASoulsPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
