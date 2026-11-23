#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "InputTriggers.h"
#include "LECombatInput.generated.h"

class UInputAction;

/** 将增强输入的动作及触发阶段映射为单条战斗意图。 */
USTRUCT(BlueprintType)
struct LEABILITYFRAMEWORK_API FLECombatInputBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Combat Input")
    TObjectPtr<const UInputAction> Action;

    UPROPERTY(EditAnywhere, Category = "Combat Input")
    ETriggerEvent TriggerEvent = ETriggerEvent::Started;

    UPROPERTY(EditAnywhere, Category = "Combat Input")
    FGameplayTag IntentTag;

    UPROPERTY(EditAnywhere, Category = "Combat Input")
    int32 Priority = 0;

    // 将动作和事件阶段绑定到接收者，并随回调传递意图配置。
    template <typename ReceiverType>
    uint32 Bind(UEnhancedInputComponent& Input, ReceiverType* Receiver,
        void (ReceiverType::*Callback)(const FInputActionValue&, FGameplayTag, int32)) const
    {
        return Input.BindAction(Action.Get(), TriggerEvent, Receiver, Callback, IntentTag, Priority).GetHandle();
    }
};

/** 保存增强输入处理后需要交给玩法的意图和方向。 */
struct FLECombatIntent
{
    FGameplayTag Tag;
    FVector Direction = FVector::ZeroVector;
};

/** 将同一次输入处理中的候选意图按固定优先级归并。 */
class LEABILITYFRAMEWORK_API FLECombatInputQueue
{
public:
    // 记录候选意图，相同优先级时保留最先收到的候选。
    void Submit(FGameplayTag Tag, const FVector& Direction, int32 Priority);

    // 取出本次输入处理中选中的意图，并清空候选。
    TOptional<FLECombatIntent> Consume();

    // 在输入模式切换或 Pawn 销毁时清空候选。
    void Reset();

private:
    TOptional<FLECombatIntent> Candidate;
    int32 CandidatePriority = 0;
};
