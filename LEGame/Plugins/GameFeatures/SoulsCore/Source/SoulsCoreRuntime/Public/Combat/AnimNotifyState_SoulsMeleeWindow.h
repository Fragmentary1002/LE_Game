#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_SoulsMeleeWindow.generated.h"

/** 动画只声明有效窗口，实际运行状态和伤害均由角色能力管理。 */
UCLASS(meta = (DisplayName = "Souls 近战命中窗口"))
class SOULSCORERUNTIME_API UAnimNotifyState_SoulsMeleeWindow : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    // 动画窗口开始时打开当前攻击的武器采样。
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

    // 动画窗口结束时停止采样，能力取消仍有独立清理兜底。
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

    // 为动画编辑器提供清晰的命中窗口名称。
    virtual FString GetNotifyName_Implementation() const override;
};
