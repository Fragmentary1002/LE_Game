#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIFrameworkAdapterLibrary.generated.h"

/** 提供 UI 适配布局计算的 Blueprint 工具函数。 */
UCLASS()
class LYRAUIFRAMEWORK_API ULyraUIFrameworkAdapterLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 根据适配策略返回内容区域四周应留出的黑边尺寸。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Adapter")
    static FMargin CalculateBlackBarPadding(FVector2D ViewportSize, FVector2D DesignResolution, ELyraUIFrameworkBlackType BlackType);
};
