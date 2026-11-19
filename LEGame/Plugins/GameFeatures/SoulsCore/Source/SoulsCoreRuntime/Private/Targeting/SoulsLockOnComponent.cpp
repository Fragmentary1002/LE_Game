#include "Targeting/SoulsLockOnComponent.h"

#include "Character/LyraHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Targeting/SoulsTargetComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsLockOnComponent)

// 默认关闭检查，获取目标后再启用。
USoulsLockOnComponent::USoulsLockOnComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 再次按下解除锁定，否则选择视野中心附近的有效目标。
void USoulsLockOnComponent::ToggleLockOn()
{
    if (CurrentTarget.IsValid())
    {
        ClearTarget();
        return;
    }
    USoulsTargetComponent* BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    const APawn* Pawn = GetPawn<APawn>();
    if (!Pawn || !Pawn->IsLocallyControlled())
    {
        return;
    }
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
    if (Health && (Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f))
    {
        return;
    }
    const APlayerController* PC = GetController<APlayerController>();
    if (!PC)
    {
        return;
    }
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    for (const TWeakObjectPtr<USoulsTargetComponent>& Entry : GetWorld()->GetSubsystem<USoulsTargetRegistry>()->GetTargets())
    {
        FVector2D Screen;
        USoulsTargetComponent* Candidate = Entry.Get();
        if (CanAcquire(Candidate, Screen))
        {
            const FVector ToTarget = Candidate->GetComponentLocation() - ViewLocation;
            const float Score = 1.0f - FVector::DotProduct(ViewRotation.Vector(), ToTarget.GetSafeNormal()) +
                0.15f * FVector::Distance(Pawn->GetActorLocation(), Candidate->GetComponentLocation()) / AcquireDistance;
            if (Score < BestScore)
            {
                BestScore = Score;
                BestTarget = Candidate;
            }
        }
    }
    SetTarget(BestTarget);
}

// 在指定屏幕方向中优先选择与当前目标接近的对象。
void USoulsLockOnComponent::SwitchTarget(float Direction)
{
    const APlayerController* PC = GetController<APlayerController>();
    if (!PC || !CurrentTarget.IsValid() || FMath::IsNearlyZero(Direction))
    {
        return;
    }
    FVector2D CurrentScreen;
    if (!PC->ProjectWorldLocationToScreen(CurrentTarget->GetComponentLocation(), CurrentScreen, true))
    {
        return;
    }
    USoulsTargetComponent* BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (const TWeakObjectPtr<USoulsTargetComponent>& Entry : GetWorld()->GetSubsystem<USoulsTargetRegistry>()->GetTargets())
    {
        FVector2D Screen;
        USoulsTargetComponent* Candidate = Entry.Get();
        if (Candidate != CurrentTarget.Get() && CanAcquire(Candidate, Screen))
        {
            const FVector2D Offset = Screen - CurrentScreen;
            const float Score = FMath::Abs(Offset.X) + 0.5f * FMath::Abs(Offset.Y);
            if (Offset.X * Direction > 5.0f && Score < BestScore)
            {
                BestScore = Score;
                BestTarget = Candidate;
            }
        }
    }
    if (BestTarget)
    {
        SetTarget(BestTarget);
    }
}

// 清除当前目标。
void USoulsLockOnComponent::ClearTarget()
{
    SetTarget(nullptr);
}

// 返回锁定对象，供 HUD 和蓝图读取。
USoulsTargetComponent* USoulsLockOnComponent::GetTarget() const
{
    return CurrentTarget.Get();
}

// 返回锁定点所属的 Actor。
AActor* USoulsLockOnComponent::GetTargetActor() const
{
    return CurrentTarget.IsValid() ? CurrentTarget->GetOwner() : nullptr;
}

// 获取目标时检查距离、视角、屏幕投影和可见性。
bool USoulsLockOnComponent::CanAcquire(const USoulsTargetComponent* Target, FVector2D& OutScreenPosition) const
{
    const APawn* Pawn = GetPawn<APawn>();
    const APlayerController* PC = GetController<APlayerController>();
    if (!Pawn || !PC || !Target || !Target->IsAvailable() || Target->GetOwner() == Pawn ||
        FVector::DistSquared(Pawn->GetActorLocation(), Target->GetComponentLocation()) > FMath::Square(AcquireDistance))
    {
        return false;
    }
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector Direction = (Target->GetComponentLocation() - ViewLocation).GetSafeNormal();
    if (FVector::DotProduct(ViewRotation.Vector(), Direction) < FMath::Cos(FMath::DegreesToRadians(AcquireHalfAngle)))
    {
        return false;
    }
    int32 Width = 0;
    int32 Height = 0;
    PC->GetViewportSize(Width, Height);
    return PC->ProjectWorldLocationToScreen(Target->GetComponentLocation(), OutScreenPosition, true) &&
        OutScreenPosition.X >= 0 && OutScreenPosition.Y >= 0 && OutScreenPosition.X <= Width &&
        OutScreenPosition.Y <= Height && HasLineOfSight(Target);
}

// 视线忽略玩家和被检测目标本身。
bool USoulsLockOnComponent::HasLineOfSight(const USoulsTargetComponent* Target) const
{
    if (!Target)
    {
        return false;
    }
    const APawn* Pawn = GetPawn<APawn>();
    if (!Pawn)
    {
        return false;
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SoulsLockVisibility), false, Pawn);
    Params.AddIgnoredActor(Target->GetOwner());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(),
        Target->GetComponentLocation(), ECC_Visibility, Params);
}

// 更换锁定对象，并对称维护对象离开世界的监听。
void USoulsLockOnComponent::SetTarget(USoulsTargetComponent* Target)
{
    if (AActor* Previous = GetTargetActor())
    {
        Previous->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleTargetEndPlay);
    }
    CurrentTarget = Target;
    OccludedTime = 0.0f;
    SetComponentTickEnabled(Target != nullptr);
    if (AActor* Next = GetTargetActor())
    {
        Next->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleTargetEndPlay);
    }
    OnTargetChanged.Broadcast(GetTargetActor());
}

// 锁定对象被销毁或卸载时立即恢复自由模式。
void USoulsLockOnComponent::HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
    ClearTarget();
}

// 锁定期间应用距离迟滞及短暂遮挡容忍。
void USoulsLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const APawn* Pawn = GetPawn<APawn>();
    const USoulsTargetComponent* Target = CurrentTarget.Get();
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
    if (!Pawn || !Pawn->IsLocallyControlled() || !Target || !Target->IsAvailable() ||
        (Health && (Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)) ||
        FVector::DistSquared(Pawn->GetActorLocation(), Target->GetComponentLocation()) >
            FMath::Square(AcquireDistance * RetainDistanceMultiplier))
    {
        ClearTarget();
        return;
    }
    OccludedTime = HasLineOfSight(Target) ? 0.0f : OccludedTime + DeltaTime;
    if (OccludedTime > OcclusionGracePeriod)
    {
        ClearTarget();
    }
}

// 组件卸载时释放锁定和目标监听。
void USoulsLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearTarget();
    Super::EndPlay(EndPlayReason);
}
