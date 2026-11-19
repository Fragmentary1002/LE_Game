#include "Character/SoulsCharacter.h"

#include "Character/SoulsHeroComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Targeting/SoulsLockOnComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsCharacter)

// 默认面向移动方向，镜头旋转不会带动静止角色旋转。
ASoulsCharacter::ASoulsCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SoulsHero = CreateDefaultSubobject<USoulsHeroComponent>(TEXT("SoulsHero"));
    LockOn = CreateDefaultSubobject<USoulsLockOnComponent>(TEXT("SoulsLockOn"));
    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->bUseControllerDesiredRotation = false;
    Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    Movement->MaxWalkSpeed = 450.0f;
    Movement->MaxAcceleration = 1800.0f;
    Movement->BrakingDecelerationWalking = 1600.0f;
}

// 链接 Lyra 的无武器动作层，后续可替换为近战武器动作层。
void ASoulsCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (UnarmedAnimationLayer)
    {
        GetMesh()->LinkAnimClassLayers(UnarmedAnimationLayer);
    }
}
