#include "LyraUIFrameworkManagerSubsystem.h"

#include "CommonLocalPlayer.h"
#include "CommonUIExtensions.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "LyraUIFrameworkLayerHost.h"
#include "LyraUIFrameworkSettings.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIView.h"
#include "LyraUIViewHandle.h"
#include "PrimaryGameLayout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkManagerSubsystem)

/** 根据配置快照执行重复策略并开始异步加载。 */
ULyraUIViewHandle* ULyraUIFrameworkManagerSubsystem::OpenView(FName UIName, UObject* OpenContext)
{
    if (bShuttingDown || !Cast<UCommonLocalPlayer>(GetLocalPlayer()))
    {
        return nullptr;
    }
    const FLyraUIConfigRow* Row = FindConfig(UIName);
    if (!Row || Row->WBPPath.IsNull() || !Row->UILayer.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("UI 配置无效：%s"), *UIName.ToString());
        return nullptr;
    }
    const FLyraUIConfigRow Config = *Row;
    const TArray<TObjectPtr<ULyraUIViewHandle>> Snapshot = ActiveHandles;
    for (ULyraUIViewHandle* Existing : Snapshot)
    {
        if (!Existing || Existing->GetUIName() != UIName || Existing->GetState() == ELyraUIFrameworkViewState::Closing)
        {
            continue;
        }
        if (Config.DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::Reuse ||
            Config.DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::BringToFront)
        {
            if (Config.DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::BringToFront)
            {
                if (ULyraUIFrameworkLayerHost* Host = LayerHosts.FindRef(Existing->Config.UILayer))
                {
                    Host->BringViewToFront(Existing->GetView());
                }
            }
            return Existing;
        }
        if (Config.DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::Restart)
        {
            CloseView(Existing, true);
        }
    }
    if (Config.DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::Restart)
    {
        for (ULyraUIViewHandle* Existing : ActiveHandles)
        {
            if (Existing && Existing->GetUIName() == UIName && Existing->GetState() != ELyraUIFrameworkViewState::Closing)
            {
                return Existing;
            }
        }
    }
    UPrimaryGameLayout* Root = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer());
    if (!Root || !Root->GetLayerWidget(Config.UILayer))
    {
        UE_LOG(LogTemp, Error, TEXT("RootLayout 未注册 UI Layer：%s"), *Config.UILayer.ToString());
        return nullptr;
    }
    ULyraUIViewHandle* Handle = NewObject<ULyraUIViewHandle>(this);
    Handle->Initialize(this, UIName, Config, OpenContext);
    Handle->SetState(ELyraUIFrameworkViewState::Loading);
    ActiveHandles.Add(Handle);
    if (Config.bSuspendInputUntilLoaded)
    {
        Handle->InputSuspendToken = UCommonUIExtensions::SuspendInputForPlayer(GetLocalPlayer(), TEXT("LyraUIFrameworkLoading"));
    }
    const TWeakObjectPtr<ULyraUIViewHandle> WeakHandle(Handle);
    TSharedPtr<FStreamableHandle> Streaming = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
        Config.WBPPath.ToSoftObjectPath(),
        FStreamableDelegate::CreateWeakLambda(this, [this, WeakHandle]()
        {
            ULyraUIViewHandle* Active = WeakHandle.Get();
            if (!Active || Active->GetState() != ELyraUIFrameworkViewState::Loading || !ActiveHandles.Contains(Active))
            {
                return;
            }
            ResumeInputForHandle(Active);
            UClass* ViewClass = Active->Config.WBPPath.Get();
            if (!ViewClass || !ViewClass->IsChildOf<ULyraUIView>() || ViewClass->HasAnyClassFlags(CLASS_Abstract))
            {
                FailOpen(Active);
                return;
            }
            ULyraUIFrameworkLayerHost* Host = GetOrCreateLayerHost(Active->Config.UILayer);
            ULyraUIView* View = Host ? CreateWidget<ULyraUIView>(GetLocalPlayer(), ViewClass) : nullptr;
            if (!ActiveHandles.Contains(Active) || Active->GetState() != ELyraUIFrameworkViewState::Loading)
            {
                RemoveEmptyLayerHost(Active->Config.UILayer, Host);
                return;
            }
            if (!View)
            {
                FailOpen(Active);
                return;
            }
            Active->SetView(View);
            View->InitializeView(Active->GetUIName(), Active->OpenContext, Active->Config, Active);
            if (!ActiveHandles.Contains(Active) || Active->GetState() != ELyraUIFrameworkViewState::Loading)
            {
                return;
            }
            Active->SetState(ELyraUIFrameworkViewState::Open);
            Host->AddView(View, Active->Config);
            if (!ActiveHandles.Contains(Active) ||
                (Active->GetState() != ELyraUIFrameworkViewState::Open && Active->GetState() != ELyraUIFrameworkViewState::Suspended))
            {
                return;
            }
            View->NotifyFrameworkOpened();
            if (ActiveHandles.Contains(Active) &&
                (Active->GetState() == ELyraUIFrameworkViewState::Open || Active->GetState() == ELyraUIFrameworkViewState::Suspended))
            {
                Active->OnOpened.Broadcast(View);
            }
        }));
    Handle->SetStreamableHandle(Streaming);
    if (!Streaming)
    {
        FailOpen(Handle);
    }
    return Handle;
}

/** 关闭先标记状态再执行业务回调，关闭动画结束后才最终移除。 */
void ULyraUIFrameworkManagerSubsystem::CloseView(ULyraUIViewHandle* Handle, bool bImmediate)
{
    if (!Handle || !ActiveHandles.Contains(Handle))
    {
        return;
    }
    if (Handle->GetState() == ELyraUIFrameworkViewState::Closing)
    {
        if (bImmediate) { FinalizeCloseView(Handle); }
        return;
    }
    Handle->SetState(ELyraUIFrameworkViewState::Closing);
    ResumeInputForHandle(Handle);
    if (Handle->StreamableHandle && Handle->StreamableHandle->IsLoading())
    {
        Handle->StreamableHandle->CancelHandle();
    }
    if (ULyraUIView* View = Handle->GetView())
    {
        View->NotifyFrameworkClosing();
        if (!ActiveHandles.Contains(Handle)) { return; }
        if (!bImmediate && !bShuttingDown && View->PlayCloseAnimation()) { return; }
    }
    FinalizeCloseView(Handle);
}

/** 在回调前清除管理器所有权，允许业务安全地关闭或再次打开。 */
void ULyraUIFrameworkManagerSubsystem::FinalizeCloseView(ULyraUIViewHandle* Handle)
{
    if (!Handle || !ActiveHandles.Contains(Handle)) { return; }
    ULyraUIView* View = Handle->GetView();
    ActiveHandles.RemoveSingle(Handle);
    Handle->SetState(ELyraUIFrameworkViewState::Closed);
    if (View)
    {
        View->ReleaseFrameworkView();
        if (ULyraUIFrameworkLayerHost* Host = LayerHosts.FindRef(Handle->Config.UILayer))
        {
            Host->RemoveView(View);
            RemoveEmptyLayerHost(Handle->Config.UILayer, Host);
        }
    }
    Handle->SetView(nullptr);
    Handle->OpenContext = nullptr;
    Handle->StreamableHandle.Reset();
    Handle->OnClosed.Broadcast(View);
}

/** 通过实例定位对应打开请求。 */
void ULyraUIFrameworkManagerSubsystem::CloseViewByWidget(ULyraUIView* View)
{
    const TArray<TObjectPtr<ULyraUIViewHandle>> Snapshot = ActiveHandles;
    for (ULyraUIViewHandle* Handle : Snapshot)
    {
        if (Handle && Handle->GetView() == View)
        {
            CloseView(Handle);
            return;
        }
    }
}

/** 根据 Layer 遮挡同步句柄的打开和暂停状态。 */
void ULyraUIFrameworkManagerSubsystem::NotifyViewVisibilityChanged(ULyraUIView* View, bool bVisible)
{
    for (ULyraUIViewHandle* Handle : ActiveHandles)
    {
        if (Handle && Handle->GetView() == View && Handle->GetState() != ELyraUIFrameworkViewState::Closing)
        {
            Handle->SetState(bVisible ? ELyraUIFrameworkViewState::Open : ELyraUIFrameworkViewState::Suspended);
            return;
        }
    }
}

/** 使用快照关闭当前实例，默认保留配置为常驻的界面。 */
void ULyraUIFrameworkManagerSubsystem::CloseAll(bool bCloseResidentViews)
{
    const TArray<TObjectPtr<ULyraUIViewHandle>> Snapshot = ActiveHandles;
    for (ULyraUIViewHandle* Handle : Snapshot)
    {
        if (Handle && (bCloseResidentViews || !Handle->Config.bResident))
        {
            CloseView(Handle, bShuttingDown);
        }
    }
}

/** 配置表以行名作为主键，拒绝不兼容的行结构。 */
const FLyraUIConfigRow* ULyraUIFrameworkManagerSubsystem::FindConfig(FName UIName) const
{
    UDataTable* Table = GetDefault<ULyraUIFrameworkSettings>()->UIConfigTable.LoadSynchronous();
    if (!Table || Table->GetRowStruct() != FLyraUIConfigRow::StaticStruct())
    {
        return nullptr;
    }
    return Table->FindRow<FLyraUIConfigRow>(UIName, TEXT("LyraUIFramework::FindConfig"), false);
}

/** 销毁时禁止回调新开界面，并立即取消加载和动画。 */
void ULyraUIFrameworkManagerSubsystem::Deinitialize()
{
    bShuttingDown = true;
    CloseAll(true);
    LayerHosts.Empty();
    Super::Deinitialize();
}

/** 将失败状态写入句柄后再广播，避免失败回调影响在途请求。 */
void ULyraUIFrameworkManagerSubsystem::FailOpen(ULyraUIViewHandle* Handle)
{
    if (!Handle || !ActiveHandles.Contains(Handle)) { return; }
    ResumeInputForHandle(Handle);
    ActiveHandles.RemoveSingle(Handle);
    Handle->SetState(ELyraUIFrameworkViewState::Failed);
    Handle->OpenContext = nullptr;
    RemoveEmptyLayerHost(Handle->Config.UILayer, LayerHosts.FindRef(Handle->Config.UILayer));
    Handle->OnFailed.Broadcast(nullptr);
}

/** 成对恢复加载期间挂起的玩家输入。 */
void ULyraUIFrameworkManagerSubsystem::ResumeInputForHandle(ULyraUIViewHandle* Handle)
{
    if (Handle && !Handle->InputSuspendToken.IsNone())
    {
        const FName Token = Handle->InputSuspendToken;
        Handle->InputSuspendToken = NAME_None;
        UCommonUIExtensions::ResumeInputForPlayer(GetLocalPlayer(), Token);
    }
}

/** 在已经注册的 Lyra Layer 上创建一个内部 Canvas 宿主。 */
ULyraUIFrameworkLayerHost* ULyraUIFrameworkManagerSubsystem::GetOrCreateLayerHost(FGameplayTag LayerTag)
{
    if (ULyraUIFrameworkLayerHost* Host = LayerHosts.FindRef(LayerTag)) { return Host; }
    UPrimaryGameLayout* Root = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer());
    ULyraUIFrameworkLayerHost* Host = Root
        ? Root->PushWidgetToLayerStack<ULyraUIFrameworkLayerHost>(LayerTag, ULyraUIFrameworkLayerHost::StaticClass())
        : nullptr;
    if (Host) { LayerHosts.Add(LayerTag, Host); }
    return Host;
}

/** 从 RootLayout 移除不再承载界面的空宿主。 */
void ULyraUIFrameworkManagerSubsystem::RemoveEmptyLayerHost(FGameplayTag LayerTag, ULyraUIFrameworkLayerHost* Host)
{
    if (!Host || Host->HasViews()) { return; }
    if (LayerHosts.FindRef(LayerTag) == Host)
    {
        LayerHosts.Remove(LayerTag);
    }
    if (UPrimaryGameLayout* Root = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer()))
    {
        Root->FindAndRemoveWidgetFromLayer(Host);
    }
}
