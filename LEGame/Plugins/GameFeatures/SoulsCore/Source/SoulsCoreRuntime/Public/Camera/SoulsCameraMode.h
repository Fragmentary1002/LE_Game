#pragma once

#include "CoreMinimal.h"
#include "Camera/LyraCameraMode_ThirdPerson.h"
#include "SoulsCameraMode.generated.h"

/** 居中第三人称相机，继续使用 Lyra 的碰撞避让。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsCameraMode : public ULyraCameraMode_ThirdPerson
{
    GENERATED_BODY()

public:
    // 设置适合近战观察的镜头距离和高度。
    USoulsCameraMode();
};

/** 在同一相机栈内平滑观察当前锁定点。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsCameraMode_LockOn : public USoulsCameraMode
{
    GENERATED_BODY()

public:
    // 设置锁定镜头参数。
    USoulsCameraMode_LockOn();
    // 激活时从现有控制旋转开始过渡。
    virtual void OnActivation() override;

protected:
    // 更新目标旋转后复用第三人称偏移与防穿墙。
    virtual void UpdateView(float DeltaTime) override;
    // 返回本帧平滑后的锁定旋转。
    virtual FRotator GetPivotRotation() const override;

    UPROPERTY(EditDefaultsOnly, Category = "Souls|Camera", meta = (ClampMin = "0.1"))
    float TrackingSpeed = 6.0f;
private:
    FRotator TrackingRotation = FRotator::ZeroRotator;
};
