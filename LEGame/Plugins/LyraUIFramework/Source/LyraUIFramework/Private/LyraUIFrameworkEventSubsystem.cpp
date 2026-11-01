#include "LyraUIFrameworkEventSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkEventSubsystem)

/** 绑定并登记脚本事件回调。 */
bool ULyraUIFrameworkEventSubsystem::AddListener(FGameplayTag EventTag, UObject* Listener, FName FunctionName)
{
    if (!EventTag.IsValid() || !IsValid(Listener) || FunctionName.IsNone())
    {
        return false;
    }

    FLyraUIFrameworkEventDelegate Delegate;
    Delegate.BindUFunction(Listener, FunctionName);
    if (!Delegate.IsBound())
    {
        return false;
    }

    TArray<FLyraUIFrameworkEventDelegate>& EventListeners = Listeners.FindOrAdd(EventTag);
    if (!EventListeners.Contains(Delegate))
    {
        EventListeners.Add(MoveTemp(Delegate));
    }
    return true;
}

/** 移除指定对象在该标签下注册的回调。 */
void ULyraUIFrameworkEventSubsystem::RemoveListener(FGameplayTag EventTag, UObject* Listener)
{
    if (!EventTag.IsValid() || !Listener)
    {
        return;
    }

    if (TArray<FLyraUIFrameworkEventDelegate>* EventListeners = Listeners.Find(EventTag))
    {
        EventListeners->RemoveAll([Listener](const FLyraUIFrameworkEventDelegate& Delegate)
        {
            return Delegate.GetUObject() == Listener;
        });
        if (EventListeners->IsEmpty())
        {
            Listeners.Remove(EventTag);
        }
    }
}

/** 复制监听列表后逐个广播，允许回调中增删监听者。 */
void ULyraUIFrameworkEventSubsystem::TriggerEvent(FGameplayTag EventTag, const FLyraUIFrameworkEventData& EventData)
{
    const TArray<FLyraUIFrameworkEventDelegate>* EventListeners = Listeners.Find(EventTag);
    if (!EventListeners)
    {
        return;
    }

    const TArray<FLyraUIFrameworkEventDelegate> ListenerSnapshot = *EventListeners;
    for (const FLyraUIFrameworkEventDelegate& Delegate : ListenerSnapshot)
    {
        Delegate.ExecuteIfBound(EventData);
    }
}

/** 清空子系统管理的事件监听。 */
void ULyraUIFrameworkEventSubsystem::Deinitialize()
{
    Listeners.Empty();
    Super::Deinitialize();
}
