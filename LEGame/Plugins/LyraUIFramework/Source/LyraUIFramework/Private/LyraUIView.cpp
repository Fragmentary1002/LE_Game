#include "LyraUIView.h"

#include "LyraUIFrameworkManagerSubsystem.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIView)

/** 记录界面标识与打开上下文。 */
void ULyraUIView::InitializeView(FName InUIName, UObject* InOpenContext)
{
    UIName = InUIName;
    OpenContext = InOpenContext;
}

/** 更新框架可见状态并触发生命周期回调。 */
void ULyraUIView::SetFrameworkVisible(bool bVisible)
{
    if (bFrameworkVisible == bVisible)
    {
        return;
    }

    bFrameworkVisible = bVisible;
    SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (bVisible)
    {
        OnResume();
    }
    else
    {
        OnPause();
    }
}

/** 通过所属 LocalPlayer 的 UI Manager 请求关闭。 */
void ULyraUIView::RequestClose()
{
    if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
    {
        if (ULyraUIFrameworkManagerSubsystem* Manager = LocalPlayer->GetSubsystem<ULyraUIFrameworkManagerSubsystem>())
        {
            Manager->CloseViewByWidget(this);
        }
    }
}

/** 确保嵌入宿主的界面也执行一次首次打开生命周期。 */
void ULyraUIView::NotifyFrameworkOpened()
{
    if (!bOpened)
    {
        bOpened = true;
        OnOpen();
    }
}

/** Widget 构造时只执行一次初始化事件。 */
void ULyraUIView::NativeConstruct()
{
    Super::NativeConstruct();

    if (!bInitialized)
    {
        bInitialized = true;
        OnInit();
    }
}

/** Widget 销毁时通知业务释放界面资源。 */
void ULyraUIView::NativeDestruct()
{
    OnRelease();
    Super::NativeDestruct();
}

/** 激活界面并区分首次打开和覆盖恢复。 */
void ULyraUIView::NativeOnActivated()
{
    Super::NativeOnActivated();

    if (!bOpened)
    {
        NotifyFrameworkOpened();
    }
    else if (bHasBeenActivated)
    {
        OnResume();
    }
    bHasBeenActivated = true;
}

/** 停用界面时通知业务暂停。 */
void ULyraUIView::NativeOnDeactivated()
{
    OnPause();
    Super::NativeOnDeactivated();
}
