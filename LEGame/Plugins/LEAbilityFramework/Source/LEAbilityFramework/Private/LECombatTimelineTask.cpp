#include "LECombatTimelineTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LECombatTimelineTask)

// 要求窗口起止时间有限且持续时间大于零。
bool FLECombatTimelineWindow::IsValid() const
{
    return Tag.IsValid() && FMath::IsFinite(StartTime) && FMath::IsFinite(EndTime) &&
        StartTime >= 0.0f && EndTime > StartTime;
}

// 启用由 GameplayTasks 管理的 Tick。
ULECombatTimelineTask::ULECombatTimelineTask(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bTickingTask = true;
}

// 生成排序后的边界列表，连续的同标签窗口先开后关。
ULECombatTimelineTask* ULECombatTimelineTask::Create(UGameplayAbility* Ability,
    TConstArrayView<FLECombatTimelineWindow> Windows, TConstArrayView<FLECombatTimelinePoint> Points, float PlayRate)
{
    ULECombatTimelineTask* Task = NewAbilityTask<ULECombatTimelineTask>(Ability);
    Task->Rate = FMath::IsFinite(PlayRate) && PlayRate > 0.0f ? PlayRate : 1.0f;
    for (const FLECombatTimelineWindow& Window : Windows)
    {
        if (Window.IsValid())
        {
            Task->Events.Add({Window.StartTime, Window.Tag, EEventType::OpenWindow});
            Task->Events.Add({Window.EndTime, Window.Tag, EEventType::CloseWindow});
        }
    }
    for (const FLECombatTimelinePoint& Point : Points)
    {
        if (Point.Tag.IsValid() && FMath::IsFinite(Point.Time) && Point.Time >= 0.0f)
        {
            Task->Events.Add({Point.Time, Point.Tag, EEventType::Point});
        }
    }
    // 同一时刻先更新窗口，再派发点事件。
    Task->Events.StableSort([](const FTimedEvent& Left, const FTimedEvent& Right)
    {
        return Left.Time != Right.Time ? Left.Time < Right.Time : Left.Type < Right.Type;
    });
    return Task;
}

// 让零秒事件在启动时立即生效。
void ULECombatTimelineTask::Activate()
{
    TickTask(0.0f);
}

// 大步长按时序遍历，任务在回调中结束时停止派发。
void ULECombatTimelineTask::TickTask(float DeltaTime)
{
    Position += FMath::Max(DeltaTime, 0.0f) * Rate;
    while (!IsFinished() && ShouldBroadcastAbilityTaskDelegates() &&
        Events.IsValidIndex(NextEventIndex) && Events[NextEventIndex].Time <= Position)
    {
        const FTimedEvent& Event = Events[NextEventIndex++];
        Dispatch(Event);
    }
    if (!IsFinished() && !Events.IsValidIndex(NextEventIndex))
    {
        EndTask();
    }
}

// 重叠的同标签窗口在最后一段关闭时才发出关闭回调。
void ULECombatTimelineTask::Dispatch(const FTimedEvent& Event)
{
    switch (Event.Type)
    {
    case EEventType::Point:
        OnPointReached.Broadcast(Event.Tag);
        break;
    case EEventType::OpenWindow:
        if (++ActiveWindows.FindOrAdd(Event.Tag) == 1)
        {
            OnWindowChanged.Broadcast(Event.Tag, true);
        }
        break;
    case EEventType::CloseWindow:
        if (int32* Count = ActiveWindows.Find(Event.Tag); Count && --*Count == 0)
        {
            ActiveWindows.Remove(Event.Tag);
            OnWindowChanged.Broadcast(Event.Tag, false);
        }
        break;
    }
}

// 转移窗口集合后再派发关闭，避免重入时重复关闭。
void ULECombatTimelineTask::OnDestroy(bool bInOwnerFinished)
{
    const TMap<FGameplayTag, int32> RemainingWindows = MoveTemp(ActiveWindows);
    ActiveWindows.Reset();
    const FLECombatTimelineWindowChanged CloseWindows = OnWindowChanged;
    OnWindowChanged.Clear();
    OnPointReached.Clear();
    Super::OnDestroy(bInOwnerFinished);
    for (const TPair<FGameplayTag, int32>& Window : RemainingWindows)
    {
        CloseWindows.Broadcast(Window.Key, false);
    }
}
