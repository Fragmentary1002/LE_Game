#pragma once

#include "UI/LyraHUD.h"
#include "SoulsHUD.generated.h"

/** 为 Souls 提供 HUD 原生扩展点，沿用 Lyra 的组件生命周期和调试显示。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsHUD : public ALyraHUD
{
    GENERATED_BODY()

public:
    // 沿用 Lyra 的 HUD 初始化。
    ASoulsHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
