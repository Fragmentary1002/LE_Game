#include "Progression/SoulDrops/SoulsSoulDrop.h"

#include "Character/LyraHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsSoulDrop)

// 默认以小球显示掉魂位置，避免对移动或战斗碰撞产生影响。
ASoulsSoulDrop::ASoulsSoulDrop()
{
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    SetRootComponent(Visual);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetGenerateOverlapEvents(false);
    Visual->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Visual->SetStaticMesh(Sphere.Object);
    Visual->SetRelativeScale3D(FVector(0.25f));
}

// 投影一经绑定便保留原 ID，使 GameMode 能拒绝过期的掉落实例。
void ASoulsSoulDrop::InitializeDrop(const FGuid& InDropId)
{
    if (HasAuthority() && !DropId.IsValid() && InDropId.IsValid())
    {
        DropId = InDropId;
    }
}

// 提供掉魂记录 ID 供 GameMode 和蓝图读取。
FGuid ASoulsSoulDrop::GetDropId() const
{
    return DropId;
}

// 仅允许同一世界内、仍被玩家占有且存活的近距离 Pawn 请求回收。
bool ASoulsSoulDrop::CanUseBy(const APawn& Pawn) const
{
    if (!HasAuthority() || IsActorBeingDestroyed() || !DropId.IsValid() || Pawn.GetWorld() != GetWorld())
    {
        return false;
    }

    const APlayerController* Player = Pawn.GetController<APlayerController>();
    if (!Player || Player->GetPawn() != &Pawn)
    {
        return false;
    }

    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(&Pawn);
    return Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f
        && FVector::DistSquared(Pawn.GetActorLocation(), GetActorLocation()) <= FMath::Square(UseDistance);
}
