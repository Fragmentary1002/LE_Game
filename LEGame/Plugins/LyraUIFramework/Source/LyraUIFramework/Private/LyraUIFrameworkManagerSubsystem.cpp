#include "LyraUIFrameworkManagerSubsystem.h"

#include "CommonLocalPlayer.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameUIPolicy.h"
#include "GameUIManagerSubsystem.h"
#include "LyraUIFrameworkSettings.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIView.h"
#include "LyraUIViewHandle.h"
#include "PrimaryGameLayout.h"
#include "Widgets/CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

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
            if (UPrimaryGameLayout* ExistingRootLayout = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer()))
            {
                if (UCommonActivatableWidgetContainerBase* LayerWidget = ExistingRootLayout->GetLayerWidget(ConfigRow->UILayer))
                {
                    LayerWidget->RemoveWidget(*ExistingView);
                    LayerWidget->AddWidgetInstance(*ExistingView);
                }
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

    const TWeakObjectPtr<ULyraUIViewHandle> WeakHandle(Handle);
    const TWeakObjectPtr<ULyraUIFrameworkManagerSubsystem> WeakManager(this);
    const FName UINameCopy = UIName;
    const TWeakObjectPtr<UObject> WeakOpenContext(OpenContext);
    TSharedPtr<FStreamableHandle> StreamableHandle = RootLayout->PushWidgetToLayerStackAsync<ULyraUIView>(
        ConfigRow->UILayer,
        ConfigRow->bSuspendInputUntilLoaded,
        TSoftClassPtr<UCommonActivatableWidget>(ConfigRow->WBPPath.ToSoftObjectPath()),
        [WeakManager, WeakHandle, UINameCopy, WeakOpenContext](EAsyncWidgetLayerState State, ULyraUIView* LoadedView)
        {
            if (ULyraUIFrameworkManagerSubsystem* Manager = WeakManager.Get())
            {
                if (State == EAsyncWidgetLayerState::Initialize && LoadedView)
                {
                    LoadedView->InitializeView(UINameCopy, WeakOpenContext.Get());
                    if (ULyraUIViewHandle* ActiveHandle = WeakHandle.Get())
                    {
                        ActiveHandle->SetView(LoadedView);
                    }
                }
                else
                {
                    Manager->HandleAsyncOpenState(WeakHandle, State, LoadedView);
                }
            }
        });

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
    if (Handle->StreamableHandle.IsValid() && Handle->StreamableHandle->IsLoading())
    {
        Handle->StreamableHandle->CancelHandle();
    }

    if (ULyraUIView* View = Handle->GetView())
    {
        View->OnClose();
        if (UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer()))
        {
            RootLayout->FindAndRemoveWidgetFromLayer(View);
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

/** 处理 CommonGame Layer 的异步推入结果。 */
void ULyraUIFrameworkManagerSubsystem::HandleAsyncOpenState(TWeakObjectPtr<ULyraUIViewHandle> WeakHandle, EAsyncWidgetLayerState AsyncState, ULyraUIView* View)
{
    ULyraUIViewHandle* Handle = WeakHandle.Get();
    if (!Handle || Handle->GetState() != ELyraUIFrameworkViewState::Loading)
    {
        return;
    }

    if (AsyncState == EAsyncWidgetLayerState::AfterPush && View)
    {
        Handle->SetView(View);
        Handle->SetState(ELyraUIFrameworkViewState::Open);
        if (Handle->Config.bRequestFocus)
        {
            View->ActivateWidget();
        }
        Handle->OnOpened.Broadcast(View);
        return;
    }

    FailOpen(Handle);
}

/** 结束失败句柄并通知监听者。 */
void ULyraUIFrameworkManagerSubsystem::FailOpen(ULyraUIViewHandle* Handle)
{
    if (!Handle)
    {
        return;
    }

    Handle->SetState(ELyraUIFrameworkViewState::Failed);
    Handle->OnFailed.Broadcast(Handle->GetView());
    ActiveHandles.RemoveSingle(Handle);
}
