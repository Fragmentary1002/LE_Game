#include "LEAbilityDefinition.h"

#include "Animation/AnimMontage.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEAbilityDefinition)

// 必需标签全部满足且禁止标签均不存在时允许继续激活。
bool FLEAbilityInteraction::CanActivate(const FGameplayTagContainer& OwnedTags) const
{
    return OwnedTags.HasAll(ActivationRequiredTags) && !OwnedTags.HasAny(ActivationBlockedTags);
}

#if WITH_EDITOR
// 报告技能引用、播放参数和时间轴区间的配置错误。
EDataValidationResult ULEAbilityDefinition::IsDataValid(FDataValidationContext& Context) const
{
    if (!AbilityTag.IsValid() || !AbilityClass || !Execution.Montage)
    {
        Context.AddError(NSLOCTEXT("LEAbilityFramework", "RequiredAbilityFields", "请配置技能标签、GA 类和 Montage。"));
    }
    if (!FMath::IsFinite(Execution.PlayRate) || Execution.PlayRate <= 0.0f)
    {
        Context.AddError(NSLOCTEXT("LEAbilityFramework", "InvalidPlayRate", "Montage 播放倍率必须大于零。"));
    }

    const float Length = Execution.Montage ? Execution.Montage->GetPlayLength() : TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < TimelineWindows.Num(); ++Index)
    {
        const FLECombatTimelineWindow& Window = TimelineWindows[Index];
        if (!Window.IsValid() || Window.EndTime > Length + KINDA_SMALL_NUMBER)
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("LEAbilityFramework", "InvalidAbilityWindow", "第 {0} 个窗口需要有效标签及 Montage 范围内的开始和结束时间。"), Index + 1));
        }
    }
    for (int32 Index = 0; Index < TimelinePoints.Num(); ++Index)
    {
        const FLECombatTimelinePoint& Point = TimelinePoints[Index];
        if (!Point.Tag.IsValid() || !FMath::IsFinite(Point.Time) || Point.Time < 0.0f || Point.Time > Length + KINDA_SMALL_NUMBER)
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("LEAbilityFramework", "InvalidAbilityPoint", "第 {0} 个点事件需要有效标签及 Montage 范围内的时间。"), Index + 1));
        }
    }
    return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
