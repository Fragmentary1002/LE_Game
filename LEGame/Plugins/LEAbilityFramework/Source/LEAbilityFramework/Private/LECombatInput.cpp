#include "LECombatInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LECombatInput)

// 仅在候选有效且优先级更高时替换当前意图。
void FLECombatInputQueue::Submit(FGameplayTag Tag, const FVector& Direction, int32 Priority)
{
    if (Tag.IsValid() && (!Candidate.IsSet() || Priority > CandidatePriority))
    {
        Candidate = FLECombatIntent{Tag, Direction};
        CandidatePriority = Priority;
    }
}

// 清空候选后再交给调用方执行，允许后续独立提交。
TOptional<FLECombatIntent> FLECombatInputQueue::Consume()
{
    TOptional<FLECombatIntent> Result = MoveTemp(Candidate);
    Reset();
    return Result;
}

// 清空本轮待处理的输入意图。
void FLECombatInputQueue::Reset()
{
    Candidate.Reset();
}
