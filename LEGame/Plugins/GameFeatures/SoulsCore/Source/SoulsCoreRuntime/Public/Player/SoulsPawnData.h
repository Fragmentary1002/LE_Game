#pragma once

#include "CoreMinimal.h"
#include "Character/LyraPawnData.h"
#include "SoulsPawnData.generated.h"

class USoulsCameraMode_LockOn;

/** 提供可独立保存和调整的魂类默认角色配置。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsPawnData : public ULyraPawnData
{
    GENERATED_BODY()

public:
    // 配置原生角色、输入和默认相机。
    USoulsPawnData(const FObjectInitializer& ObjectInitializer);

    // 允许不同角色配置独立的锁定相机蓝图。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Camera")
    TSubclassOf<USoulsCameraMode_LockOn> LockOnCameraMode;
};
