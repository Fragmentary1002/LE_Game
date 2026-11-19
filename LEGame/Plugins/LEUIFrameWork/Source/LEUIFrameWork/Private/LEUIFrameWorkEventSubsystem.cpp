#include "LEUIFrameWorkEventSubsystem.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEUIFrameWorkEventSubsystem)

/** 绑定并登记脚本事件回调。 */
bool ULEUIFrameWorkEventSubsystem::AddListener(FGameplayTag EventTag, UObject* Listener, FName FunctionName)
{
    if (!EventTag.IsValid() || !IsValid(Listener) || FunctionName.IsNone())
    {
        return false;
    }

    UFunction* Function = Listener->FindFunction(FunctionName);
    if (!Function || Function->NumParms != 1)
    {
        return false;
    }
    bool bMatchesPayload = false;
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_Parm))
        {
            const FStructProperty* Parameter = CastField<FStructProperty>(*It);
            bMatchesPayload = Parameter && Parameter->Struct == FLEUIFrameWorkEventData::StaticStruct() &&
                !It->HasAnyPropertyFlags(CPF_ReturnParm) &&
                (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ConstParm));
        }
    }
    if (!bMatchesPayload) { return false; }

    FLEUIFrameWorkEventDelegate Delegate;
    Delegate.BindUFunction(Listener, FunctionName);
    if (!Delegate.IsBound())
    {
        return false;
    }

    TArray<FLEUIFrameWorkEventDelegate>& EventListeners = Listeners.FindOrAdd(EventTag);
    if (!EventListeners.Contains(Delegate))
    {
        EventListeners.Add(MoveTemp(Delegate));
    }
    return true;
}

/** 移除指定对象在该标签下注册的回调。 */
void ULEUIFrameWorkEventSubsystem::RemoveListener(FGameplayTag EventTag, UObject* Listener)
{
    if (!EventTag.IsValid() || !Listener)
    {
        return;
    }

    if (TArray<FLEUIFrameWorkEventDelegate>* EventListeners = Listeners.Find(EventTag))
    {
        EventListeners->RemoveAll([Listener](const FLEUIFrameWorkEventDelegate& Delegate)
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
void ULEUIFrameWorkEventSubsystem::TriggerEvent(FGameplayTag EventTag, const FLEUIFrameWorkEventData& EventData)
{
    const TArray<FLEUIFrameWorkEventDelegate>* EventListeners = Listeners.Find(EventTag);
    if (!EventListeners)
    {
        return;
    }

    const TArray<FLEUIFrameWorkEventDelegate> ListenerSnapshot = *EventListeners;
    for (const FLEUIFrameWorkEventDelegate& Delegate : ListenerSnapshot)
    {
        Delegate.ExecuteIfBound(EventData);
    }
}

/** 清空子系统管理的事件监听。 */
void ULEUIFrameWorkEventSubsystem::Deinitialize()
{
    Listeners.Empty();
    Super::Deinitialize();
}
