#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyraUIFrameworkModelPreviewActor.generated.h"

class AActor;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** 仅捕获指定 Actor 并输出 RenderTarget 的 UI 模型预览场景。 */
UCLASS(BlueprintType)
class LYRAUIFRAMEWORK_API ALyraUIFrameworkModelPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    /** 创建根场景捕获组件并关闭逐帧自动捕获。 */
    ALyraUIFrameworkModelPreviewActor(const FObjectInitializer& ObjectInitializer);

    /** 创建仅显示指定模型的预览 RenderTarget。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Model Preview")
    void SetPreviewActor(AActor* InActor, int32 Width = 512, int32 Height = 512);

    /** 获取供 UImage 或 UI 材质使用的渲染目标。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Model Preview")
    UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

    /** 设置模型预览相机的水平和垂直角度。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Model Preview")
    void SetPreviewOrbit(float Yaw, float Pitch);

    /** 按需执行一次场景捕获。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Model Preview")
    void CapturePreview();

    /** 停止捕获并释放当前模型引用。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Model Preview")
    void ClearPreview();

protected:
    /** 释放 show-only 列表和动态 RenderTarget。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 根据模型包围球设置相机位置并朝向模型中心。 */
    void UpdateCameraTransform();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI Model", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneCaptureComponent2D> SceneCapture;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "UI Model", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTextureRenderTarget2D> RenderTarget;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> PreviewActor;

    FVector PreviewCenter = FVector::ZeroVector;
    float PreviewRadius = 100.0f;
    float OrbitYaw = 0.0f;
    float OrbitPitch = 0.0f;
};
