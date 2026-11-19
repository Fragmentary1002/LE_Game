#include "Camera/SoulsCameraMode.h"

#include "Targeting/SoulsLockOnComponent.h"
#include "Targeting/SoulsTargetComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsCameraMode)

// 使用运行时曲线配置居中镜头，保留第三人称碰撞检查。
USoulsCameraMode::USoulsCameraMode()
{
    FieldOfView = 80.0f;
    ViewPitchMin = -65.0f;
    ViewPitchMax = 65.0f;
    BlendTime = 0.25f;
    bUseRuntimeFloatCurves = true;
    TargetOffsetX.GetRichCurve()->AddKey(-90.0f, -360.0f);
    TargetOffsetX.GetRichCurve()->AddKey(90.0f, -360.0f);
    TargetOffsetY.GetRichCurve()->AddKey(0.0f, 0.0f);
    TargetOffsetZ.GetRichCurve()->AddKey(0.0f, 30.0f);
}

// 锁定镜头沿用自由相机距离，缩短切换过渡时间。
USoulsCameraMode_LockOn::USoulsCameraMode_LockOn()
{
    BlendTime = 0.2f;
}

// 从当前视角开始跟随，避免首次锁定发生跳变。
void USoulsCameraMode_LockOn::OnActivation()
{
    Super::OnActivation();
    TrackingRotation = Super::GetPivotRotation();
}

// 向目标锁定点平滑旋转，并继续执行碰撞避让。
void USoulsCameraMode_LockOn::UpdateView(float DeltaTime)
{
    const AActor* Player = GetTargetActor();
    const USoulsLockOnComponent* Lock = Player ? Player->FindComponentByClass<USoulsLockOnComponent>() : nullptr;
    if (Lock && Lock->GetTarget())
    {
        const FVector Pivot = GetPivotLocation();
        const FVector Focus = Lock->GetTarget()->GetComponentLocation();
        FRotator Desired = (Focus - Pivot).Rotation();
        Desired.Pitch = FMath::ClampAngle(Desired.Pitch, -35.0f, 35.0f);
        Desired.Roll = 0.0f;
        TrackingRotation = FMath::RInterpTo(TrackingRotation, Desired, DeltaTime, TrackingSpeed);
    }
    Super::UpdateView(DeltaTime);
}

// 相机基类使用此旋转计算镜头和控制器朝向。
FRotator USoulsCameraMode_LockOn::GetPivotRotation() const
{
    return TrackingRotation;
}
