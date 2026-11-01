#include "LyraUIFrameworkManagerSubsystem.h"

#include "CommonLocalPlayer.h"
#include "CommonUIExtensions.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameUIPolicy.h"
#include "GameUIManagerSubsystem.h"
#include "LyraUIFrameworkLayerHost.h"
#include "LyraUIFrameworkSettings.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIView.h"
#include "LyraUIViewHandle.h"
#include "PrimaryGameLayout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkManagerSubsystem)

/** 根据配置执行重复策略并异步将界面压入 Lyra Layer。 */
ULyraUIViewHandle* ULyraUIFrameworkManagerSubsystem::OpenView(FName UIName, UObject* OpenContext)
{
    const FLyraUIConfigRow* ConfigRow = FindConfig(UIName);
    if (!ConfigRow || ConfigRow->WBPPath.IsNull() || !ConfigRow->UILayer.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("UI 配置无效，无法打开界面：%s"), *UIName.ToString());
        return nullptr;
    }

    TArray<ULyraUIViewHandle*> ExistingHandles;
    for (ULyraUIViewHandle* ExistingHandle : ActiveHandles)
    {
        if (ExistingHandle && ExistingHandle->GetUIName() == UIName &&
            ExistingHandle->GetState() != ELyraUIFrameworkViewState::Closed &&
            ExistingHandle->GetState() != ELyraUIFrameworkViewState::Failed)
        {
            ExistingHandles.Add(ExistingHandle);
        }
    }

    if (ConfigRow->DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::Reuse && !ExistingHandles.IsEmpty())
    {
        return ExistingHandles[0];
    }

    if (ConfigRow->DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::BringToFront && !ExistingHandles.IsEmpty())
    {
        ULyraUIViewHandle* ExistingHandle = ExistingHandles[0];
        if (ULyraUIView* ExistingView = ExistingHandle->GetView())
        {
            if (ULyraUIFrameworkLayerHost* LayerHost = LayerHosts.FindRef(ConfigRow->UILayer))
            {
                LayerHost->RemoveView(ExistingView);
                LayerHost->AddView(ExistingView, ExistingHandle->Config.Presentation, ExistingHandle->Config.ZOrder);
            }
        }
        return ExistingHandle;
    }

    if (ConfigRow->DuplicatePolicy == ELyraUIFrameworkDuplicatePolicy::Restart)
    {
        for (ULyraUIViewHandle* ExistingHandle : ExistingHandles)
        {
            CloseView(ExistingHandle);
        }
    }

    UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer());
    if (!RootLayout)
    {
        UE_LOG(LogTemp, Warning, TEXT("RootLayout 尚未创建，无法打开界面：%s"), *UIName.ToString());
        return nullptr;
    }

    ULyraUIViewHandle* Handle = NewObject<ULyraUIViewHandle>(this);
    Handle->Initialize(this, UIName, *ConfigRow, OpenContext);
    Handle->SetState(ELyraUIFrameworkViewState::Loading);
    ActiveHandles.Add(Handle);
    if (Handle->Config.bSuspendInputUntilLoaded)
    {
        Handle->InputSuspendToken = UCommonUIExtensions::SuspendInputForPlayer(GetLocalPlayer(), TEXT("LyraUIFrameworkLoading"));
    }

    const TWeakObjectPtr<ULyraUIViewHandle> WeakHandle(Handle);
    const TWeakObjectPtr<ULyraUIFrameworkManagerSubsystem> WeakManager(this);
    TSharedPtr<FStreamableHandle> StreamableHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
        ConfigRow->WBPPath.ToSoftObjectPath(),
        FStreamableDelegate::CreateWeakLambda(this, [WeakManager, WeakHandle]()
        {
            if (ULyraUIFrameworkManagerSubsystem* Manager = WeakManager.Get())
            {
                ULyraUIViewHandle* ActiveHandle = WeakHandle.Get();
                if (!ActiveHandle || ActiveHandle->GetState() != ELyraUIFrameworkViewState::Loading)
                {
                    return;
                }
                Manager->ResumeInputForHandle(ActiveHandle);

                UClass* ViewClass = ActiveHandle->Config.WBPPath.Get();
                ULyraUIFrameworkLayerHost* LayerHost = Manager->GetOrCreateLayerHost(ActiveHandle->Config.UILayer);
                ULyraUIView* LoadedView = ViewClass && LayerHost
                    ? CreateWidget<ULyraUIView>(Manager->GetLocalPlayer(), ViewClass)
                    : nullptr;
                if (!LoadedView)
                {
                    Manager->FailOpen(ActiveHandle);
                    return;
                }

                LoadedView->InitializeView(ActiveHandle->GetUIName(), ActiveHandle->OpenContext.Get());
                ActiveHandle->SetView(LoadedView);
                LayerHost->AddView(LoadedView, ActiveHandle->Config.Presentation, ActiveHandle->Config.ZOrder);
                ActiveHandle->SetState(ELyraUIFrameworkViewState::Open);
                if (ActiveHandle->Config.bRequestFocus)
                {
                    LoadedView->ActivateWidget();
                }
                ActiveHandle->OnOpened.Broadcast(LoadedView);
            }
        }));

    Handle->SetStreamableHandle(MoveTemp(StreamableHandle));
    return Handle;
}

/** 取消未完成加载并从 RootLayout 移除指定界面。 */
void ULyraUIFrameworkManagerSubsystem::CloseView(ULyraUIViewHandle* Handle)
{
    if (!Handle || !ActiveHandles.Contains(Handle))
    {
        return;
    }

    Handle->SetState(ELyraUIFrameworkViewState::Closing);
    ResumeInputForHandle(Handle);
    if (Handle->StreamableHandle.IsValid() && Handle->StreamableHandle->IsLoading())
    {
        Handle->StreamableHandle->CancelHandle();
    }

    if (ULyraUIView* View = Handle->GetView())
    {
        View->OnClose();
        if (ULyraUIFrameworkLayerHost* LayerHost = LayerHosts.FindRef(Handle->Config.UILayer))
        {
            LayerHost->RemoveView(View);
        }
    }

    Handle->OnClosed.Broadcast(Handle->GetView());
    Handle->SetState(ELyraUIFrameworkViewState::Closed);
    Handle->SetView(nullptr);
    ActiveHandles.RemoveSingle(Handle);
}

/** 按界面对象定位句柄并关闭。 */
void ULyraUIFrameworkManagerSubsystem::CloseViewByWidget(ULyraUIView* View)
{
    if (!View)
    {
        return;
    }

    for (ULyraUIViewHandle* Handle : ActiveHandles)
    {
        if (Handle && Handle->GetView() == View)
        {
            CloseView(Handle);
            return;
        }
    }
}

/** 按配置保留常驻界面或关闭全部界面。 */
void ULyraUIFrameworkManagerSubsystem::CloseAll(bool bCloseResidentViews)
{
    const TArray<TObjectPtr<ULyraUIViewHandle>> HandlesToClose = ActiveHandles;
    for (ULyraUIViewHandle* Handle : HandlesToClose)
    {
        if (Handle && (bCloseResidentViews || !Handle->Config.bResident))
        {
            CloseView(Handle);
        }
    }
}

/** 从项目设置指定的 DataTable 中查找 UI 配置行。 */
const FLyraUIConfigRow* ULyraUIFrameworkManagerSubsystem::FindConfig(FName UIName) const
{
    const ULyraUIFrameworkSettings* Settings = GetDefault<ULyraUIFrameworkSettings>();
    UDataTable* ConfigTable = Settings ? Settings->UIConfigTable.LoadSynchronous() : nullptr;
    if (!ConfigTable)
    {
        return nullptr;
    }

    return ConfigTable->FindRow<FLyraUIConfigRow>(UIName, TEXT("LyraUIFrameworkManagerSubsystem::FindConfig"), false);
}

/** 子系统销毁前释放仍在加载或打开的界面。 */
void ULyraUIFrameworkManagerSubsystem::Deinitialize()
{
    CloseAll(true);
    Super::Deinitialize();
}

/** 结束失败句柄并通知监听者。 */
void ULyraUIFrameworkManagerSubsystem::FailOpen(ULyraUIViewHandle* Handle)
{
    if (!Handle)
    {
        return;
    }

    ResumeInputForHandle(Handle);
    Handle->SetState(ELyraUIFrameworkViewState::Failed);
    Handle->OnFailed.Broadcast(Handle->GetView());
    ActiveHandles.RemoveSingle(Handle);
}

/** 使用 CommonUI token 恢复指定句柄挂起的玩家输入。 */
void ULyraUIFrameworkManagerSubsystem::ResumeInputForHandle(ULyraUIViewHandle* Handle)
{
    if (Handle && !Handle->InputSuspendToken.IsNone())
    {
        UCommonUIExtensions::ResumeInputForPlayer(GetLocalPlayer(), Handle->InputSuspendToken);
        Handle->InputSuspendToken = NAME_None;
    }
}

/** 获取已创建的宿主；缺少时将宿主压入指定 Lyra Layer。 */
ULyraUIFrameworkLayerHost* ULyraUIFrameworkManagerSubsystem::GetOrCreateLayerHost(FGameplayTag LayerTag)
{
    if (ULyraUIFrameworkLayerHost* ExistingHost = LayerHosts.FindRef(LayerTag))
    {
        return ExistingHost;
    }

    UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer());
    ULyraUIFrameworkLayerHost* NewHost = RootLayout
        ? RootLayout->PushWidgetToLayerStack<ULyraUIFrameworkLayerHost>(LayerTag, ULyraUIFrameworkLayerHost::StaticClass())
        : nullptr;
    if (NewHost)
    {
        LayerHosts.Add(LayerTag, NewHost);
    }
    return NewHost;
}
