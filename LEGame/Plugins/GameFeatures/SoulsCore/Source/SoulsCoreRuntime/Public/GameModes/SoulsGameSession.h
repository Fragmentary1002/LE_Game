#pragma once

#include "System/LyraGameSession.h"
#include "SoulsGameSession.generated.h"

/** 为 Souls 提供游戏会话原生扩展点，沿用 Lyra 的登录和比赛生命周期。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsGameSession : public ALyraGameSession
{
    GENERATED_BODY()

public:
    // 沿用 Lyra 的会话初始化。
    ASoulsGameSession(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
