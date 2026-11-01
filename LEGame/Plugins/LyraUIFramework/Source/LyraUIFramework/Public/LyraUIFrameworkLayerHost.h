#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIFrameworkLayerHost.generated.h"

class UCanvasPanel;
class ULyraUIView;
class UCanvasPanelSlot;

/** 在 Lyra 的单个 CommonUI Layer 内排序并管理多个框架界面。 */
UCLASS()
class LYRAUIFRAMEWORK_API ULyraUIFrameworkLayerHost : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 将 View 放入 Canvas 并按配置重排同层界面。 */
    void AddView(ULyraUIView* View, ELyraUIFrameworkViewPresentation Presentation, int32 ZOrder);

    /** 从该 Layer 移除指定 View 并恢复剩余界面的可见状态。 */
    void RemoveView(ULyraUIView* View);

protected:
    /** 创建承载 Layer View 的全屏 Canvas。 */
    virtual void NativeOnInitialized() override;

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

    /** 运行时创建的全屏 Canvas。 */
    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> CanvasPanel;
};
