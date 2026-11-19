#pragma once

#include "CoreMinimal.h"
#include "Input/LyraInputConfig.h"
#include "SoulsInputConfig.generated.h"

class UInputMappingContext;

/** 集中配置魂类默认按键，资产可通过编辑器独立保存。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsInputConfig : public ULyraInputConfig
{
    GENERATED_BODY()

public:
    // 创建默认输入动作和键鼠、手柄映射。
    USoulsInputConfig(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Input")
    TObjectPtr<UInputMappingContext> MappingContext;
};
