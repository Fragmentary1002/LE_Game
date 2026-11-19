#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "LEUIFrameWorkTypes.h"
#include "LEUIFrameWorkLayerHost.generated.h"

class UCanvasPanel;
class UBorder;
class ULEUIView;

/** 一个同层界面及其布局配置，避免并行数组在回调重入时错位。 */
USTRUCT()
struct FLEUIFrameWorkLayerEntry
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<ULEUIView> View = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> Background = nullptr;

    UPROPERTY(Transient)
    FLEUIConfigRow Config;

    int64 Order = 0;
};

/** 由 CommonUI 管理外层输入栈、由 Canvas 管理同层可见窗口。 */
UCLASS()
class LEUIFRAMEWORK_API ULEUIFrameWorkLayerHost : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 插入同层界面并同步遮挡状态。 */
    void AddView(ULEUIView* View, const FLEUIConfigRow& Config);

    /** 移除指定实例，刷新后恢复下层内容。 */
    void RemoveView(ULEUIView* View);

    /** 原地调整排序，不移除控件或触发释放。 */
    void BringViewToFront(ULEUIView* View);

    /** 判断宿主是否仍被界面使用。 */
    bool HasViews() const { return !Entries.IsEmpty(); }

protected:
    /** 创建原生宿主 Canvas。 */
    virtual void NativeOnInitialized() override;

    /** 使用真实布局尺寸响应窗口大小和 DPI 变化。 */
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    /** 使用快照更新槽位和遮挡，允许业务回调增删界面。 */
    void RefreshViews();

    UPROPERTY(Transient)
    TArray<FLEUIFrameWorkLayerEntry> Entries;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> CanvasPanel;

    FVector2D CachedViewportSize = FVector2D::ZeroVector;
    int64 NextOrder = 0;
    bool bRefreshing = false;
    bool bRefreshRequested = false;
};
