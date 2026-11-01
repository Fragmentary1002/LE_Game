#include "LyraUIFrameworkModelPreviewActor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkModelPreviewActor)

/** 创建根场景捕获组件并关闭逐帧自动捕获。 */
ALyraUIFrameworkModelPreviewActor::ALyraUIFrameworkModelPreviewActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("UIModelSceneCapture"));
    SetRootComponent(SceneCapture);
    SceneCapture->bCaptureEveryFrame = false;
    SceneCapture->bCaptureOnMovement = false;
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
}

/** 创建仅显示指定模型的预览 RenderTarget。 */
void ALyraUIFrameworkModelPreviewActor::SetPreviewActor(AActor* InActor, int32 Width, int32 Height)
{
    ClearPreview();
    if (!IsValid(InActor) || !SceneCapture)
    {
        return;
    }

    PreviewActor = InActor;
    FVector BoundsOrigin;
    FVector BoundsExtent;
    InActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
    PreviewCenter = BoundsOrigin;
    PreviewRadius = FMath::Max(BoundsExtent.Size(), 1.0f);

    RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    RenderTarget->ClearColor = FLinearColor::Transparent;
    RenderTarget->InitAutoFormat(FMath::Clamp(Width, 16, 4096), FMath::Clamp(Height, 16, 4096));
    SceneCapture->TextureTarget = RenderTarget;
    SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    SceneCapture->ShowOnlyActorComponents(InActor, true);
    UpdateCameraTransform();
    CapturePreview();
}

/** 设置模型预览相机的水平和垂直角度。 */
void ALyraUIFrameworkModelPreviewActor::SetPreviewOrbit(float Yaw, float Pitch)
{
    OrbitYaw = Yaw;
    OrbitPitch = FMath::Clamp(Pitch, -80.0f, 80.0f);
    UpdateCameraTransform();
    CapturePreview();
}

/** 按需执行一次场景捕获。 */
void ALyraUIFrameworkModelPreviewActor::CapturePreview()
{
    if (PreviewActor.IsValid() && SceneCapture && RenderTarget)
    {
        SceneCapture->CaptureScene();
    }
}

/** 停止捕获并释放当前模型引用。 */
void ALyraUIFrameworkModelPreviewActor::ClearPreview()
{
    if (SceneCapture)
    {
        SceneCapture->ClearShowOnlyComponents();
        SceneCapture->TextureTarget = nullptr;
    }
    PreviewActor.Reset();
    RenderTarget = nullptr;
}

/** 释放 show-only 列表和动态 RenderTarget。 */
void ALyraUIFrameworkModelPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPreview();
    Super::EndPlay(EndPlayReason);
}

/** 根据模型包围球设置相机位置并朝向模型中心。 */
void ALyraUIFrameworkModelPreviewActor::UpdateCameraTransform()
{
    if (!SceneCapture || !PreviewActor.IsValid())
    {
        return;
    }
    const float HorizontalHalfFov = FMath::DegreesToRadians(SceneCapture->FOVAngle * 0.5f);
    const float AspectRatio = RenderTarget ? static_cast<float>(RenderTarget->SizeX) / FMath::Max(1, RenderTarget->SizeY) : 1.0f;
    const float VerticalHalfFov = FMath::Atan(FMath::Tan(HorizontalHalfFov) / FMath::Max(AspectRatio, 0.01f));
    const float NarrowHalfFov = FMath::Min(HorizontalHalfFov, VerticalHalfFov);
    const float Distance = PreviewRadius / FMath::Max(FMath::Sin(NarrowHalfFov), 0.01f) * 1.1f;
    const FRotator OrbitRotation(OrbitPitch, OrbitYaw, 0.0f);
    const FVector CameraOffset = OrbitRotation.RotateVector(FVector(Distance, 0.0f, 0.0f));
    const FVector CameraLocation = PreviewCenter + CameraOffset;
    SceneCapture->SetWorldLocation(CameraLocation);
    SceneCapture->SetWorldRotation((PreviewCenter - CameraLocation).Rotation());
}
