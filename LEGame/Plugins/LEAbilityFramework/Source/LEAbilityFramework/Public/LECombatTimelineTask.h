#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "LECombatTimelineTask.generated.h"

/** 以招式逻辑秒数配置一个持续窗口。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLECombatTimelineWindow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Combat Timeline")
    FGameplayTag Tag;

    UPROPERTY(EditAnywhere, Category = "Combat Timeline", meta = (ClampMin = "0"))
    float StartTime = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Combat Timeline", meta = (ClampMin = "0"))
    float EndTime = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Combat Timeline")
    FText DisplayName;

    // 校验窗口标签及有效时段。
    bool IsValid() const;
};

/** 在逻辑时间轴经过指定位置时派发一次事件。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLECombatTimelinePoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Combat Timeline")
    FGameplayTag Tag;

    UPROPERTY(EditAnywhere, Category = "Combat Timeline", meta = (ClampMin = "0"))
    float Time = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Combat Timeline")
    FText DisplayName;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FLECombatTimelineWindowChanged, FGameplayTag, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FLECombatTimelinePointReached, FGameplayTag);

/** 由本次 GameplayAbility 持有，按逻辑时间推进事件点和窗口边界。 */
UCLASS()
class LEABILITYFRAMEWORK_API ULECombatTimelineTask : public UAbilityTask
{
    GENERATED_BODY()

public:
    // 在 GameplayTasks 组件上启用任务 Tick。
    ULECombatTimelineTask(const FObjectInitializer& ObjectInitializer);

    // 按时间排序边界，待调用方绑定回调后启动。
    static ULECombatTimelineTask* Create(UGameplayAbility* Ability, TConstArrayView<FLECombatTimelineWindow> Windows,
        TConstArrayView<FLECombatTimelinePoint> Points, float PlayRate);

    // 派发起点上的窗口和点事件。
    virtual void Activate() override;

    // 依次派发本帧跨过的所有时序事件。
    virtual void TickTask(float DeltaTime) override;

    FLECombatTimelineWindowChanged OnWindowChanged;
    FLECombatTimelinePointReached OnPointReached;

protected:
    // 关闭任务仍持有的窗口，释放生命周期绑定。
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    enum class EEventType : uint8
    {
        OpenWindow,
        CloseWindow,
        Point
    };

    struct FTimedEvent
    {
        float Time;
        FGameplayTag Tag;
        EEventType Type;
    };

    // 更新指定标签的活动窗口数量并派发开关。
    void Dispatch(const FTimedEvent& Event);

    TArray<FTimedEvent> Events;
    TMap<FGameplayTag, int32> ActiveWindows;
    double Position = 0.0;
    int32 NextEventIndex = 0;
    float Rate = 1.0f;
};
