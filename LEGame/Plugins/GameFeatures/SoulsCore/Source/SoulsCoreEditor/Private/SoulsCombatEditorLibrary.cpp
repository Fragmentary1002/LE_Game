#include "SoulsCombatEditorLibrary.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AttributeSet.h"
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsCombatEditorLibrary)

// 仅编辑 SoulsCore 复制的资源，避免改动 Lyra 公共动作或残留旧 AN_Melee。
bool USoulsCombatEditorLibrary::ConfigureAttackMontage(UAnimMontage* Montage, UAnimSequence* Sequence)
{
    UClass* WindowClass = LoadClass<UAnimNotifyState>(nullptr, TEXT("/Script/SoulsCoreRuntime.AnimNotifyState_SoulsMeleeWindow"));
    if (!Montage || !Sequence || !WindowClass || Sequence->GetPlayLength() <= 0.0f ||
        !Montage->GetPathName().StartsWith(TEXT("/SoulsCore/")) ||
        !Sequence->GetPathName().StartsWith(TEXT("/SoulsCore/")))
    {
        return false;
    }
    Sequence->Modify();
    Sequence->Notifies.Reset();
    Sequence->RefreshCacheData();
    Sequence->MarkPackageDirty();

    Montage->Modify();
    Montage->Notifies.Reset();
    Montage->SlotAnimTracks.SetNum(1);
    FSlotAnimationTrack& Track = Montage->SlotAnimTracks[0];
    Track.SlotName = TEXT("FullBody");
    Track.AnimTrack.AnimSegments.SetNum(1);
    FAnimSegment& Segment = Track.AnimTrack.AnimSegments[0];
    Segment.SetAnimReference(Sequence, true);
    Segment.StartPos = 0.0f;
    Segment.AnimStartTime = 0.0f;
    Segment.AnimEndTime = Sequence->GetPlayLength();
    Segment.AnimPlayRate = 1.0f;
    Segment.LoopingCount = 1;
    Montage->CompositeSections.SetNum(1);
    Montage->CompositeSections[0].SectionName = TEXT("Default");
    Montage->CompositeSections[0].NextSectionName = NAME_None;
    Montage->CompositeSections[0].Link(Montage, 0.0f);

    FAnimNotifyEvent& Window = Montage->Notifies.AddDefaulted_GetRef();
    Window.NotifyName = TEXT("SoulsMeleeWindow");
    Window.NotifyStateClass = NewObject<UAnimNotifyState>(Montage, WindowClass, NAME_None, RF_Transactional);
    const float Start = Sequence->GetPlayLength() * 0.2f;
    const float Duration = Sequence->GetPlayLength() * 0.35f;
    Window.Link(Montage, Start);
    Window.SetDuration(Duration);
    Window.EndLink.Link(Montage, Start + Duration);
    Window.TrackIndex = 0;
    Window.Guid = FGuid::NewGuid();
    Window.bTriggerOnDedicatedServer = true;
    Montage->RefreshCacheData();
    Montage->PostEditChange();
    Montage->MarkPackageDirty();
    return ValidateAttackMontage(Montage);
}

// 通过反射引用 Lyra 未导出的类型，保留原始队伍判定与 Damage 元属性链路。
bool USoulsCombatEditorLibrary::ConfigureCombatEffect(UBlueprint* Blueprint, bool bDamageExecution, float BaseDamage)
{
    UGameplayEffect* Effect = Blueprint && Blueprint->GeneratedClass ?
        Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!Effect)
    {
        return false;
    }
    Effect->Modify();
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    Effect->Modifiers.Reset();
    Effect->Executions.Reset();
    if (bDamageExecution)
    {
        UClass* Execution = LoadClass<UGameplayEffectExecutionCalculation>(nullptr, TEXT("/Script/LyraGame.LyraDamageExecution"));
        if (!Execution)
        {
            return false;
        }
        Effect->Executions.AddDefaulted_GetRef().CalculationClass = Execution;
    }
    else
    {
        UClass* CombatSet = LoadClass<UAttributeSet>(nullptr, TEXT("/Script/LyraGame.LyraCombatSet"));
        FProperty* DamageProperty = CombatSet ? FindFProperty<FProperty>(CombatSet, TEXT("BaseDamage")) : nullptr;
        if (!DamageProperty)
        {
            return false;
        }
        FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
        Modifier.Attribute = FGameplayAttribute(DamageProperty);
        Modifier.ModifierOp = EGameplayModOp::Override;
        Modifier.ModifierMagnitude = FScalableFloat(FMath::Max(BaseDamage, 0.0f));
    }
    Effect->PostEditChange();
    Blueprint->MarkPackageDirty();
    return true;
}

// 检查可实际播放的动作片段和唯一命中窗口，拒绝旧蓝图近战通知混入。
bool USoulsCombatEditorLibrary::ValidateAttackMontage(const UAnimMontage* Montage)
{
    if (!Montage || Montage->SlotAnimTracks.Num() != 1 || Montage->Notifies.Num() != 1 ||
        Montage->SlotAnimTracks[0].SlotName != TEXT("FullBody"))
    {
        return false;
    }
    const FAnimNotifyEvent& Window = Montage->Notifies[0];
    if (!Window.NotifyStateClass || Window.NotifyStateClass->GetClass()->GetName() != TEXT("AnimNotifyState_SoulsMeleeWindow") ||
        Window.GetDuration() <= 0.0f)
    {
        return false;
    }
    const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
    return Segments.Num() == 1 && Segments[0].GetAnimReference() &&
        Segments[0].GetAnimReference()->Notifies.IsEmpty();
}
