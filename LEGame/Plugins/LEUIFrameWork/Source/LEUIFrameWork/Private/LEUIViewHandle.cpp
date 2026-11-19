#include "LEUIViewHandle.h"

#include "LEUIFrameWorkManagerSubsystem.h"
#include "LEUIView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEUIViewHandle)

/** 保存该次打开请求的管理器、配置和业务上下文。 */
void ULEUIViewHandle::Initialize(ULEUIFrameWorkManagerSubsystem* InManager, FName InUIName, const FLEUIConfigRow& InConfig, UObject* InOpenContext)
{
    Manager = InManager;
    UIName = InUIName;
    Config = InConfig;
    OpenContext = InOpenContext;
}

/** 保存 CommonGame 返回的异步加载句柄。 */
void ULEUIViewHandle::SetStreamableHandle(TSharedPtr<FStreamableHandle> InHandle)
{
    StreamableHandle = MoveTemp(InHandle);
}

/** 更新已创建的界面实例引用。 */
void ULEUIViewHandle::SetView(ULEUIView* InView)
{
    View = InView;
}

/** 更新当前打开请求的状态。 */
void ULEUIViewHandle::SetState(ELEUIFrameWorkViewState InState)
{
    State = InState;
}

/** 将关闭请求转交给所属 UI Manager。 */
void ULEUIViewHandle::Close()
{
    if (ULEUIFrameWorkManagerSubsystem* OwningManager = Manager.Get())
    {
        OwningManager->CloseView(this);
    }
}
