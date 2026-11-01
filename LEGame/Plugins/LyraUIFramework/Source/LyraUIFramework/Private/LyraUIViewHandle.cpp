#include "LyraUIViewHandle.h"

#include "LyraUIFrameworkManagerSubsystem.h"
#include "LyraUIView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIViewHandle)

/** 保存该次打开请求的管理器、配置和业务上下文。 */
void ULyraUIViewHandle::Initialize(ULyraUIFrameworkManagerSubsystem* InManager, FName InUIName, const FLyraUIConfigRow& InConfig, UObject* InOpenContext)
{
    Manager = InManager;
    UIName = InUIName;
    Config = InConfig;
    OpenContext = InOpenContext;
}

/** 保存 CommonGame 返回的异步加载句柄。 */
void ULyraUIViewHandle::SetStreamableHandle(TSharedPtr<FStreamableHandle> InHandle)
{
    StreamableHandle = MoveTemp(InHandle);
}

/** 更新已创建的界面实例引用。 */
void ULyraUIViewHandle::SetView(ULyraUIView* InView)
{
    View = InView;
}

/** 更新当前打开请求的状态。 */
void ULyraUIViewHandle::SetState(ELyraUIFrameworkViewState InState)
{
    State = InState;
}

/** 将关闭请求转交给所属 UI Manager。 */
void ULyraUIViewHandle::Close()
{
    if (ULyraUIFrameworkManagerSubsystem* OwningManager = Manager.Get())
    {
        OwningManager->CloseView(this);
    }
}
