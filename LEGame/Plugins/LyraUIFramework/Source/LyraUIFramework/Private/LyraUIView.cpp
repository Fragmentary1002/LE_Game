#include "LyraUIView.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SafeZone.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/LocalPlayer.h"
#include "LyraUIControlBinding.h"
#include "LyraUIFrameworkManagerSubsystem.h"
#include "LyraUIFrameworkModelPreviewActor.h"
#include "LyraUIFrameworkWidgetCreationLibrary.h"
#include "LyraUISubView.h"
#include "LyraUIViewController.h"
#include "LyraUIViewHandle.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIView)

/** 确保上下文和句柄就绪后才激活界面。 */
ULyraUIView::ULyraUIView(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    bAutoActivate = false;
    bIsBackHandler = true;
}

/** 保存配置并在控件构建前完成安全区和控制器初始化。 */
void ULyraUIView::InitializeView(FName InUIName, UObject* InOpenContext, const FLyraUIConfigRow& Config, ULyraUIViewHandle* InHandle)
{
    UIName = InUIName;
    OpenContext = InOpenContext;
    ViewHandle = InHandle;
    bRequestFocus = Config.bRequestFocus;
    bIsBackHandler = Config.bCloseOnBack;
    ControlBinding = NewObject<ULyraUIControlBinding>(this);
    if (Config.bUseSafeArea)
    {
        ApplySafeArea();
    }
    if (Config.ControllerClass)
    {
        Controller = NewObject<ULyraUIViewController>(this, Config.ControllerClass);
        Controller->InitializeController(this, InOpenContext);
    }
}

/** 更新逻辑可见性并成对更新 CommonUI 激活和映射状态。 */
void ULyraUIView::SetFrameworkVisible(bool bVisible)
{
    if (bReleased || bClosing || bFrameworkVisible == bVisible)
    {
        return;
    }
    bFrameworkVisible = bVisible;
    {
        TGuardValue<bool> ActivationGuard(bUpdatingActivation, true);
        if (bVisible && bOpened)
        {
            ActivateWidget();
        }
        else if (!bVisible)
        {
            DeactivateWidget();
        }
    }
    SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (ULocalPlayer* Player = GetOwningLocalPlayer())
    {
        if (ULyraUIFrameworkManagerSubsystem* Manager = Player->GetSubsystem<ULyraUIFrameworkManagerSubsystem>())
        {
            Manager->NotifyViewVisibilityChanged(this, bVisible);
        }
    }
    if (bOpened)
    {
        if (bVisible)
        {
            if (Controller) { Controller->OnResume(); }
            if (!bClosing && !bReleased) { OnResume(); }
        }
        else
        {
            if (Controller) { Controller->OnPause(); }
            if (!bClosing && !bReleased) { OnPause(); }
        }
    }
}

/** 使用保存的句柄关闭该实例，不依赖全局 ViewClass 查找。 */
void ULyraUIView::RequestClose()
{
    if (ULyraUIViewHandle* Handle = ViewHandle.Get())
    {
        Handle->Close();
    }
}

/** 在当前本地玩家的 UIExtension 插槽注册一个界面子控件。 */
bool ULyraUIView::RegisterWidgetExtension(FGameplayTag ExtensionPointTag, TSubclassOf<UUserWidget> WidgetClass, int32 ExtensionPriority)
{
    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    UWorld* World = GetWorld();
    UUIExtensionSubsystem* ExtensionSubsystem = World ? World->GetSubsystem<UUIExtensionSubsystem>() : nullptr;
    if (!LocalPlayer || !ExtensionSubsystem || !ExtensionPointTag.IsValid() || !WidgetClass)
    {
        return false;
    }
    FUIExtensionHandle Handle = ExtensionSubsystem->RegisterExtensionAsWidgetForContext(ExtensionPointTag, LocalPlayer, WidgetClass, ExtensionPriority);
    if (!Handle.IsValid())
    {
        return false;
    }
    UIExtensionHandles.Add(MoveTemp(Handle));
    return true;
}

/** 使用项目控件工厂为当前 WidgetTree 创建 Image。 */
UImage* ULyraUIView::CreateFrameworkImage(FName WidgetName)
{
    return ULyraUIFrameworkWidgetCreationLibrary::CreateImage(WidgetTree, WidgetName);
}

/** 使用项目控件工厂为当前 WidgetTree 创建任意 UWidget 子类。 */
UWidget* ULyraUIView::CreateFrameworkWidget(TSubclassOf<UWidget> WidgetClass, FName WidgetName)
{
    return ULyraUIFrameworkWidgetCreationLibrary::CreateWidget(WidgetTree, WidgetClass, WidgetName);
}

/** 使用项目控件工厂为当前 WidgetTree 创建 TextBlock。 */
UTextBlock* ULyraUIView::CreateFrameworkTextBlock(FName WidgetName)
{
    return ULyraUIFrameworkWidgetCreationLibrary::CreateTextBlock(WidgetTree, WidgetName);
}

/** 创建模型预览 Actor 并将 RenderTarget 绑定到指定 Image。 */
ALyraUIFrameworkModelPreviewActor* ULyraUIView::CreateModelPreview(AActor* ModelActor, UImage* TargetImage, int32 Width, int32 Height)
{
    UWorld* World = GetWorld();
    if (!World || !IsValid(ModelActor) || !IsValid(TargetImage))
    {
        return nullptr;
    }
    ALyraUIFrameworkModelPreviewActor* PreviewActor = World->SpawnActor<ALyraUIFrameworkModelPreviewActor>();
    if (!PreviewActor)
    {
        return nullptr;
    }
    PreviewActor->SetPreviewActor(ModelActor, Width, Height);
    UTextureRenderTarget2D* RenderTarget = PreviewActor->GetRenderTarget();
    if (!RenderTarget)
    {
        PreviewActor->Destroy();
        return nullptr;
    }
    ModelPreviewActors.Add(PreviewActor);
    TargetImage->SetBrushResourceObject(RenderTarget);
    return PreviewActor;
}

/** 打开业务前初始化子界面，生命周期回调后重新检查关闭状态。 */
void ULyraUIView::NotifyFrameworkOpened()
{
    if (bOpened || bReleased || bClosing)
    {
        return;
    }
    bOpened = true;
    TArray<TObjectPtr<ULyraUISubView>> DiscoveredSubViews;
    WidgetTree->ForEachWidgetAndDescendants([this, &DiscoveredSubViews](UWidget* Widget)
    {
        if (ULyraUISubView* SubView = Cast<ULyraUISubView>(Widget);
            SubView && SubView->GetTypedOuter<ULyraUIView>() == this)
        {
            DiscoveredSubViews.AddUnique(SubView);
        }
    });
    for (ULyraUISubView* SubView : DiscoveredSubViews)
    {
        if (bClosing || bReleased) { return; }
        SubViews.AddUnique(SubView);
        SubView->InitializeSubView(this);
        if (bClosing || bReleased) { return; }
    }
    if (bClosing || bReleased) { return; }
    OnInit();
    if (bClosing || bReleased) { return; }
    OnAddListener();
    if (bClosing || bReleased) { return; }
    if (Controller) { Controller->OnOpen(); }
    if (bClosing || bReleased) { return; }
    OnOpen();
    if (bClosing || bReleased) { return; }
    if (bFrameworkVisible)
    {
        TGuardValue<bool> ActivationGuard(bUpdatingActivation, true);
        ActivateWidget();
        if (bRequestFocus) { RequestRefreshFocus(); }
        PlayOpenAnimation();
    }
}

/** 关闭开始后不再发送暂停回调，防止业务重复解除监听。 */
void ULyraUIView::NotifyFrameworkClosing()
{
    if (bClosing || bReleased) { return; }
    bClosing = true;
    OnRemoveListener();
    if (bReleased) { return; }
    OnClose();
    if (Controller && !bReleased) { Controller->OnClose(); }
}

/** 逻辑释放与 Slate 重建分离，确保置顶和重建不会丢失资源。 */
void ULyraUIView::ReleaseFrameworkView()
{
    if (bReleased) { return; }
    bReleased = true;
    bClosing = true;
    StopAllAnimations();
    if (CloseAnimation)
    {
        FWidgetAnimationDynamicEvent Event;
        Event.BindDynamic(this, &ThisClass::HandleCloseAnimationFinished);
        UnbindFromAnimationFinished(CloseAnimation, Event);
    }
    TGuardValue<bool> ActivationGuard(bUpdatingActivation, true);
    DeactivateWidget();
    for (ULyraUISubView* SubView : SubViews)
    {
        if (SubView) { SubView->ReleaseSubView(); }
    }
    SubViews.Empty();
    for (FUIExtensionHandle& ExtensionHandle : UIExtensionHandles)
    {
        ExtensionHandle.Unregister();
    }
    UIExtensionHandles.Empty();
    for (ALyraUIFrameworkModelPreviewActor* PreviewActor : ModelPreviewActors)
    {
        if (IsValid(PreviewActor)) { PreviewActor->Destroy(); }
    }
    ModelPreviewActors.Empty();
    if (ControlBinding) { ControlBinding->UnbindAll(); }
    if (Controller) { Controller->OnRelease(); }
    OnRelease();
    Controller = nullptr;
    OpenContext = nullptr;
    ViewHandle.Reset();
}

/** 可选打开动画在业务已初始化后播放。 */
bool ULyraUIView::PlayOpenAnimation()
{
    if (!OpenAnimation || bClosing || bReleased) { return false; }
    PlayAnimation(OpenAnimation);
    return true;
}

/** 关闭动画只绑定一次完成事件，并停止打开动画避免竞争。 */
bool ULyraUIView::PlayCloseAnimation()
{
    if (!CloseAnimation || bReleased || !bFrameworkVisible) { return false; }
    if (bClosingWithAnimation) { return true; }
    bClosingWithAnimation = true;
    StopAllAnimations();
    FWidgetAnimationDynamicEvent Event;
    Event.BindDynamic(this, &ThisClass::HandleCloseAnimationFinished);
    UnbindFromAnimationFinished(CloseAnimation, Event);
    BindToAnimationFinished(CloseAnimation, Event);
    PlayAnimation(CloseAnimation);
    return true;
}

/** 通过立即关闭入口完成已播放结束的关闭动画。 */
void ULyraUIView::HandleCloseAnimationFinished()
{
    bClosingWithAnimation = false;
    if (ULocalPlayer* Player = GetOwningLocalPlayer())
    {
        if (ULyraUIFrameworkManagerSubsystem* Manager = Player->GetSubsystem<ULyraUIFrameworkManagerSubsystem>())
        {
            Manager->CloseView(ViewHandle.Get(), true);
        }
    }
}

/** 保留蓝图自定义返回处理，默认交给统一关闭流程。 */
bool ULyraUIView::NativeOnHandleBackAction()
{
    if (!bIsBackHandler) { return false; }
    if (!BP_OnHandleBackAction()) { RequestClose(); }
    return true;
}

/** 外部直接停用视为关闭，内部遮挡停用由保护标记排除。 */
void ULyraUIView::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
    if (!bUpdatingActivation && bOpened && !bClosing && !bReleased)
    {
        RequestClose();
    }
}

/** Slate 生命周期不触发业务 OnRelease，业务由 Manager 显式回收。 */
void ULyraUIView::NativeDestruct()
{
    TGuardValue<bool> ActivationGuard(bUpdatingActivation, true);
    Super::NativeDestruct();
}

/** 安全区容器只包裹一次，UMG 原生控件负责响应设备变化。 */
void ULyraUIView::ApplySafeArea()
{
    if (WidgetTree && WidgetTree->RootWidget && !WidgetTree->RootWidget->IsA<USafeZone>())
    {
        UWidget* Content = WidgetTree->RootWidget;
        USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>();
        SafeZone->SetSidesToPad(true, true, true, true);
        SafeZone->SetContent(Content);
        WidgetTree->RootWidget = SafeZone;
    }
}
