#include "Player/SoulsCharacter.h"

#include "Player/Input/SoulsHeroComponent.h"
#include "Combat/Actions/SoulsCombatActionComponent.h"
#include "Combat/Reactions/SoulsCombatReactionComponent.h"
#include "Combat/Melee/SoulsMeleeTraceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/SoulsGameMode.h"
#include "Player/LyraPlayerState.h"
#include "Targeting/SoulsLockOnComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsCharacter)

// 默认面向移动方向，镜头旋转不会带动静止角色旋转。
ASoulsCharacter::ASoulsCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SoulsHero = CreateDefaultSubobject<USoulsHeroComponent>(TEXT("SoulsHero"));
    LockOn = CreateDefaultSubobject<USoulsLockOnComponent>(TEXT("SoulsLockOn"));
    MeleeTrace = CreateDefaultSubobject<USoulsMeleeTraceComponent>(TEXT("MeleeTrace"));
    CombatReaction = CreateDefaultSubobject<USoulsCombatReactionComponent>(TEXT("CombatReaction"));
    CombatActions = CreateDefaultSubobject<USoulsCombatActionComponent>(TEXT("CombatActions"));
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel6, ECR_Overlap);
    GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel6, ECR_Ignore);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->bUseControllerDesiredRotation = false;
    Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    Movement->MaxWalkSpeed = 450.0f;
    Movement->MaxAcceleration = 1800.0f;
    Movement->BrakingDecelerationWalking = 1600.0f;
}

// 通过 PlayerState 设置阵营，沿 Lyra 的队伍事件同步到控制器和角色。
void ASoulsCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (HasAuthority())
    {
        if (IsPlayerControlled())
        {
            // 首次有效步行更新前，使用占有时的脚下位置作为掉魂落点。
            SafeSoulDropLocation = GetCharacterMovement()->GetActorFeetLocation();
        }

        if (ALyraPlayerState* SoulsPlayerState = GetPlayerState<ALyraPlayerState>())
        {
            if (SoulsPlayerState->GetGenericTeamId() == FGenericTeamId::NoTeam)
            {
                SoulsPlayerState->SetGenericTeamId(FGenericTeamId(DefaultPlayerTeamId));
            }
        }
    }
}

// 链接 Lyra 的无武器动作层，后续可替换为近战武器动作层。
void ASoulsCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::UpdateSafeGroundLocation);
    }

    if (UnarmedAnimationLayer)
    {
        GetMesh()->LinkAnimClassLayers(UnarmedAnimationLayer);
    }
}

// 仅用普通步行模式下的可行走地面刷新落点，坠落和死亡停步时保留上一处位置。
void ASoulsCharacter::UpdateSafeGroundLocation(float, FVector, FVector)
{
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (IsPlayerControlled() && Movement->MovementMode == MOVE_Walking && Movement->CurrentFloor.IsWalkableFloor())
    {
        SafeSoulDropLocation = Movement->GetActorFeetLocation();
    }
}

// 保留 Lyra 的死亡开始处理，并在当前玩家 Pawn 解绑前报告死亡。
void ASoulsCharacter::OnDeathStarted(AActor* OwningActor)
{
    Super::OnDeathStarted(OwningActor);
    if (!HasAuthority() || OwningActor != this)
    {
        return;
    }

    APlayerController* Player = GetController<APlayerController>();
    if (!Player || Player->GetPawn() != this)
    {
        return;
    }

    if (ASoulsGameMode* GameMode = GetWorld()->GetAuthGameMode<ASoulsGameMode>())
    {
        GameMode->RecordPlayerDeath(Player, SafeSoulDropLocation);
    }
}

// 通过角色现有输入组件提供样例地图的 E 键交互。
void ASoulsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ThisClass::InputInteract);
    if (const ASoulsGameMode* Mode = GetWorld()->GetAuthGameMode<ASoulsGameMode>(); Mode && Mode->HasLevelRoute())
    {
        PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::InputRouteHeal);
    }
}

// 让权威 GameMode 统一选择并校验可交互目标。
void ASoulsCharacter::InputInteract()
{
    if (ASoulsGameMode* GameMode = GetWorld()->GetAuthGameMode<ASoulsGameMode>())
    {
        GameMode->RequestInteraction(GetController<APlayerController>());
    }
}

// 由当前权威玩法模式判断回复能否开始。
void ASoulsCharacter::InputRouteHeal()
{
    if (ASoulsGameMode* Mode = GetWorld()->GetAuthGameMode<ASoulsGameMode>())
    {
        Mode->RequestRouteHeal(GetController<APlayerController>());
    }
}
