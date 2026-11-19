#include "Targeting/SoulsTargetComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsTargetComponent)

// 登记目标并顺带清理失效条目。
void USoulsTargetRegistry::RegisterTarget(USoulsTargetComponent* Target)
{
    Targets.RemoveAll([](const TWeakObjectPtr<USoulsTargetComponent>& Entry) { return !Entry.IsValid(); });
    Targets.AddUnique(Target);
}

// 撤销目标及已失效条目。
void USoulsTargetRegistry::UnregisterTarget(USoulsTargetComponent* Target)
{
    Targets.RemoveAll([Target](const TWeakObjectPtr<USoulsTargetComponent>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Target;
    });
}

// 隐藏、销毁中或生命归零的对象不能作为锁定目标。
bool USoulsTargetComponent::IsAvailable() const
{
    const AActor* Owner = GetOwner();
    if (!bLockable || !IsValid(Owner) || Owner->IsActorBeingDestroyed() || Owner->IsHidden())
    {
        return false;
    }
    const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
    return !ASC || !ASC->HasAttributeSetForAttribute(ULyraHealthSet::GetHealthAttribute()) ||
        ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) > 0.0f;
}

// 目标进入游戏后加入世界登记表。
void USoulsTargetComponent::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->GetSubsystem<USoulsTargetRegistry>()->RegisterTarget(this);
}

// 目标离开游戏后从世界登记表移除。
void USoulsTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (USoulsTargetRegistry* Registry = GetWorld()->GetSubsystem<USoulsTargetRegistry>())
    {
        Registry->UnregisterTarget(this);
    }
    Super::EndPlay(EndPlayReason);
}
