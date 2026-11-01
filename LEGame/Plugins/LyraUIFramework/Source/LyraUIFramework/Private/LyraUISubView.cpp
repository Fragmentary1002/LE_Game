#include "LyraUISubView.h"

#include "LyraUIView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUISubView)

/** 保存父 View 并确保初始化回调只执行一次。 */
void ULyraUISubView::InitializeSubView(ULyraUIView* InOwningView)
{
    OwningView = InOwningView;
    if (!bInitialized)
    {
        bInitialized = true;
        OnInit();
    }
}

/** 显示子界面并启用其事件监听。 */
void ULyraUISubView::OpenSubView()
{
    if (bSubViewOpen || bReleased || !bInitialized || !OwningView.IsValid())
    {
        return;
    }

    bSubViewOpen = true;
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    OnAddListener();
    if (bSubViewOpen && !bReleased) { OnOpen(); }
}

/** 隐藏子界面并停用其事件监听。 */
void ULyraUISubView::CloseSubView()
{
    if (!bSubViewOpen)
    {
        return;
    }

    bSubViewOpen = false;
    OnRemoveListener();
    OnClose();
    SetVisibility(ESlateVisibility::Collapsed);
}

/** 父界面最终释放时成对结束业务资源生命周期。 */
void ULyraUISubView::ReleaseSubView()
{
    if (bReleased) { return; }
    bReleased = true;
    CloseSubView();
    OnRelease();
    OwningView.Reset();
}

/** Slate 移除仅关闭监听，最终业务释放由父界面负责。 */
void ULyraUISubView::NativeDestruct()
{
    CloseSubView();
    Super::NativeDestruct();
}
