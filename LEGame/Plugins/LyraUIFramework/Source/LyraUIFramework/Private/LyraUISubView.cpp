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
    if (bSubViewOpen)
    {
        return;
    }

    bSubViewOpen = true;
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    OnAddListener();
    OnOpen();
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

/** 销毁时关闭子界面并通知业务释放资源。 */
void ULyraUISubView::NativeDestruct()
{
    CloseSubView();
    OnRelease();
    Super::NativeDestruct();
}
