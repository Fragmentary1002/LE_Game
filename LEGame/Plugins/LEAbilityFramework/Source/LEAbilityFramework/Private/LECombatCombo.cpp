#include "LECombatCombo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LECombatCombo)

// 使用提交时间定义缓存的有效期。
void FLECombatCombo::Buffer(const FLECombatIntent& Intent, double Now, double BufferSeconds)
{
    BufferedIntent = FBufferedIntent{Intent.Tag, Intent.Direction, Now + FMath::Max(BufferSeconds, 0.0)};
}

// 供玩法层决定是否预约消费。
bool FLECombatCombo::HasBufferedInput() const
{
    return BufferedIntent.IsSet();
}

// 先清理再返回，以保证同一输入只消费一次。
TOptional<FLECombatIntent> FLECombatCombo::Consume(double Now)
{
    if (!BufferedIntent.IsSet())
    {
        return {};
    }

    const FBufferedIntent Intent = BufferedIntent.GetValue();
    Reset();
    if (Now < Intent.ExpiresAt)
    {
        return FLECombatIntent{Intent.Tag, Intent.Direction};
    }
    return {};
}

// 清空待消费的意图。
void FLECombatCombo::Reset()
{
    BufferedIntent.Reset();
}

// 相同优先级时保留传入列表中的第一条规则。
const FLECombatComboTransition* FLECombatCombo::FindTransition(TConstArrayView<FLECombatComboTransition> Transitions,
    FGameplayTag IntentTag, const FGameplayTagContainer& OpenWindows,
    const FGameplayTagContainer& OwnedTags)
{
    const FLECombatComboTransition* Best = nullptr;
    for (const FLECombatComboTransition& Transition : Transitions)
    {
        if (Transition.TriggerInputIntentTag == IntentTag && Transition.TargetComboTag.IsValid() &&
            (!Transition.RequiredWindow.IsValid() || OpenWindows.HasTagExact(Transition.RequiredWindow)) &&
            OwnedTags.HasAll(Transition.RequiredTags) && !OwnedTags.HasAny(Transition.BlockedTags) &&
            (!Best || Transition.Priority > Best->Priority))
        {
            Best = &Transition;
        }
    }
    return Best;
}
