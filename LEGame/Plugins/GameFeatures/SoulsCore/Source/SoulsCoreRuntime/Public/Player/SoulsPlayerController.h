#pragma once

#include "Player/LyraPlayerController.h"
#include "SoulsPlayerController.generated.h"

/** 为 Souls 本地玩家提供控制器扩展点，沿用 Lyra 的输入、相机与队伍接口。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsPlayerController : public ALyraPlayerController
{
    GENERATED_BODY()

public:
    // 沿用 Lyra 的玩家控制器初始化。
    ASoulsPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
