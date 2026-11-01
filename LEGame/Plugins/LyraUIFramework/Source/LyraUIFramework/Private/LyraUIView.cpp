#include "LyraUIView.h"

#include "Blueprint/WidgetTree.h"
#include "LyraUIFrameworkManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Components/SafeZone.h"
#include "Components/Widget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIView)

/** 记录界面标识与打开上下文。 */
void ULyraUIView::InitializeView(FName InUIName, UObject* InOpenContext, bool bInUseSafeArea, ELyraUIFrameworkBlackType InBlackType)
{
    UIName = InUIName;
    OpenContext = InOpenContext;
    bUseSafeArea = bInUseSafeArea;
    BlackType = InBlackType;
    ApplySafeArea();
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
        PlayOpenAnimation();
    }
}

/** 播放打开动画；未配置动画时返回 false。 */
bool ULyraUIView::PlayOpenAnimation()
{
    if (!OpenAnimation)
    {
        return false;
    }

    PlayAnimation(OpenAnimation);
    return true;
}

/** 播放关闭动画并登记完成后的关闭回调。 */
bool ULyraUIView::PlayCloseAnimation()
{
    if (!CloseAnimation || bClosingWithAnimation)
    {
        return false;
    }

    bClosingWithAnimation = true;
    FWidgetAnimationDynamicEvent FinishedEvent;
    FinishedEvent.BindDynamic(this, &ThisClass::HandleCloseAnimationFinished);
    BindToAnimationFinished(CloseAnimation, FinishedEvent);
    PlayAnimation(CloseAnimation);
    return true;
}

/** 动画完成后清理状态并请求关闭界面。 */
void ULyraUIView::HandleCloseAnimationFinished()
{
    bClosingWithAnimation = false;
    RequestClose();
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

/** 将蓝图内容根节点包装进 SafeZone，适配设备安全区。 */
void ULyraUIView::ApplySafeArea()
{
    if (bUseSafeArea && WidgetTree && WidgetTree->RootWidget && !Cast<USafeZone>(WidgetTree->RootWidget))
    {
        UWidget* ContentWidget = WidgetTree->RootWidget;
        USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("UIFrameworkSafeZone"));
        SafeZone->SetSidesToPad(true, true, true, true);
        SafeZone->SetContent(ContentWidget);
        WidgetTree->RootWidget = SafeZone;
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
