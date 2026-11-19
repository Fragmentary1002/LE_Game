#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoulsSetupLibrary.generated.h"

/** 提供首次生成资产时所需的编辑器挂载入口。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsSetupLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 首次创建 GameFeatureData 前显式挂载插件的内容目录。
    UFUNCTION(BlueprintCallable, Category = "Souls|Development")
    static bool PrepareContentForSetup();
};
