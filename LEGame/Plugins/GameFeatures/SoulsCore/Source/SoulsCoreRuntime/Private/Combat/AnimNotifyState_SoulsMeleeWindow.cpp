#include "Combat/AnimNotifyState_SoulsMeleeWindow.h"

#include "Combat/SoulsMeleeTraceComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_SoulsMeleeWindow)

// 通过当前角色的运行时组件开窗，不在共享通知对象中保存角色状态。
void UAnimNotifyState_SoulsMeleeWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
    if (AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr)
    {
        if (USoulsMeleeTraceComponent* Trace = Owner->FindComponentByClass<USoulsMeleeTraceComponent>())
        {
            Trace->SetHitWindowEnabled(true);
        }
    }
}

// 正常动画结束只关闭有效期，不重置一次攻击的命中记录。
void UAnimNotifyState_SoulsMeleeWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);
    if (AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr)
    {
        if (USoulsMeleeTraceComponent* Trace = Owner->FindComponentByClass<USoulsMeleeTraceComponent>())
        {
            Trace->SetHitWindowEnabled(false);
        }
    }
}

// 动画轨道显示用途明确的窗口标签。
FString UAnimNotifyState_SoulsMeleeWindow::GetNotifyName_Implementation() const
{
    return TEXT("Souls 近战命中窗口");
}
