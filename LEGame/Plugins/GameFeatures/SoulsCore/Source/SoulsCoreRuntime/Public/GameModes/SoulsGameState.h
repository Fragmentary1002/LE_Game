#pragma once

#include "CoreMinimal.h"
#include "GameModes/LyraGameState.h"
#include "SoulsGameState.generated.h"

/** 为魂类玩法提供局内游戏状态扩展点。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsGameState : public ALyraGameState
{
    GENERATED_BODY()

public:
    // 沿用 Lyra 的局内状态与 Experience 初始化。
    ASoulsGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
