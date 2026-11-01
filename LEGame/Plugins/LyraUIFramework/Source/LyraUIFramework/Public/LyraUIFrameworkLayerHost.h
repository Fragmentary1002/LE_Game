#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIFrameworkLayerHost.generated.h"

class UCanvasPanel;
class UBorder;
class ULyraUIView;
class UCanvasPanelSlot;

/** 在 Lyra 的单个 CommonUI Layer 内排序并管理多个框架界面。 */
UCLASS()
class LYRAUIFRAMEWORK_API ULyraUIFrameworkLayerHost : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 将 View 放入 Canvas 并按配置重排同层界面。 */
    void AddView(ULyraUIView* View, ELyraUIFrameworkViewPresentation Presentation, int32 ZOrder, ELyraUIFrameworkBlackType BlackType, FVector2D DesignResolution);

    /** 从该 Layer 移除指定 View 并恢复剩余界面的可见状态。 */
    void RemoveView(ULyraUIView* View);

protected:
    /** 创建承载 Layer View 的全屏 Canvas。 */
    virtual void NativeOnInitialized() override;

    /** 视口尺寸改变时重算黑边和内容区域。 */
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    /** 按 ZOrder 更新 Canvas 槽位并计算全屏遮挡关系。 */
    void RefreshViews();

    /** 用于反射持有界面实例，避免被 GC 回收。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULyraUIView>> Views;

    /** 全屏 View 会隐藏其下方的同层界面。 */
    UPROPERTY(Transient)
    TArray<ELyraUIFrameworkViewPresentation> Presentations;

    /** 同层排序值，数值越大越靠上。 */
    UPROPERTY(Transient)
    TArray<int32> ZOrders;

    /** 每个内容 View 对应的黑色背景控件。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> BlackBarBackgrounds;

    /** 各 View 使用的黑边策略。 */
    UPROPERTY(Transient)
    TArray<ELyraUIFrameworkBlackType> BlackTypes;

    /** 各 View 的设计分辨率。 */
    UPROPERTY(Transient)
    TArray<FVector2D> DesignResolutions;

    /** 各 View 设计分辨率，用于计算保持比例后的留边。 */
    UPROPERTY(Transient)
    TArray<FVector2D> DesignResolutions;

    /** 运行时创建的全屏 Canvas。 */
    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> CanvasPanel;

    /** 上次计算布局时的视口尺寸。 */
    FVector2D CachedViewportSize = FVector2D::ZeroVector;
};
