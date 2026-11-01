#pragma once

#include "CoreMinimal.h"
#include "GameUIPolicy.h"
#include "LyraUIFrameworkPolicy.generated.h"

/** 使用原生框架根布局，同时保留 CommonGame 的分屏和玩家生命周期。 */
UCLASS()
class LYRAUIFRAMEWORK_API ULyraUIFrameworkPolicy : public UGameUIPolicy
{
    GENERATED_BODY()

protected:
    /** 为每个本地玩家选择带有 Lyra 兼容 Layer 的框架布局。 */
    virtual TSubclassOf<UPrimaryGameLayout> GetLayoutWidgetClass(UCommonLocalPlayer* LocalPlayer) override;
};
