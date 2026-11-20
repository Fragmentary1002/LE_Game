#include "Checkpoint/SoulsBonfire.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/SoulsGameMode.h"
#include "Player/LyraPlayerStart.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsBonfire)

// 创建场景根组件，使用其位置作为交互距离与视线检测的目标点。
ASoulsBonfire::ASoulsBonfire()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("InteractionOrigin")));
    UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BonfireMarker"));
    Marker->SetupAttachment(GetRootComponent());
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Marker->SetCanEverAffectNavigation(false);
    Marker->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.75f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    Marker->SetStaticMesh(Cone.Object);
}

// 返回当前篝火实例的持久化标识。
FGuid ASoulsBonfire::GetBonfireId() const
{
    return BonfireId;
}

// 返回关卡中为该篝火指定的重生点。
ALyraPlayerStart* ASoulsBonfire::GetCheckpointStart() const
{
    return CheckpointStart.Get();
}

// 检查世界和 3D 距离，再从 Pawn 视点检测 Visibility 遮挡。
bool ASoulsBonfire::CanUseBy(const APawn& Pawn) const
{
    const UWorld* World = GetWorld();
    if (!World || IsActorBeingDestroyed() || !IsValid(&Pawn) || Pawn.IsActorBeingDestroyed() ||
        Pawn.GetWorld() != World || InteractionDistance <= 0.0f)
    {
        return false;
    }

    const FVector TargetLocation = GetActorLocation();
    if (FVector::DistSquared(Pawn.GetActorLocation(), TargetLocation) > FMath::Square(InteractionDistance))
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SoulsBonfireVisibility), false, &Pawn);
    QueryParams.AddIgnoredActor(this);
    return !World->LineTraceTestByChannel(Pawn.GetPawnViewLocation(), TargetLocation, ECC_Visibility, QueryParams);
}

// 验证控制器和受控 Pawn 后，将请求交给当前世界的权威 GameMode。
bool ASoulsBonfire::Interact(APlayerController* Player)
{
    UWorld* World = GetWorld();
    if (!World || !IsValid(Player) || Player->IsActorBeingDestroyed() || Player->GetWorld() != World)
    {
        return false;
    }

    const APawn* Pawn = Player->GetPawn();
    if (!IsValid(Pawn) || !CanUseBy(*Pawn))
    {
        return false;
    }

    ASoulsGameMode* GameMode = World->GetAuthGameMode<ASoulsGameMode>();
    return IsValid(GameMode) && GameMode->RequestBonfire(Player, this);
}

#if WITH_EDITOR
// 新放置时补齐标识，保留已配置的值。
void ASoulsBonfire::PostActorCreated()
{
    Super::PostActorCreated();

    if (IsPlacedEditorInstance() && !BonfireId.IsValid())
    {
        BonfireId = FGuid::NewGuid();
    }
}

// 为编辑器复制实例分配新标识，PIE 继续使用关卡保存的标识。
void ASoulsBonfire::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
    Super::PostDuplicate(DuplicateMode);

    if (DuplicateMode != EDuplicateMode::PIE && IsPlacedEditorInstance())
    {
        BonfireId = FGuid::NewGuid();
    }
}

// 编辑器粘贴或导入在属性复制结束后更新标识。
void ASoulsBonfire::PostEditImport()
{
    Super::PostEditImport();

    if (IsPlacedEditorInstance())
    {
        BonfireId = FGuid::NewGuid();
    }
}

// 只在可保存的编辑器关卡实例上生成持久标识。
bool ASoulsBonfire::IsPlacedEditorInstance() const
{
    const UWorld* World = GetWorld();
    return World && World->WorldType == EWorldType::Editor && !IsTemplate() && !HasAnyFlags(RF_Transient);
}
#endif
