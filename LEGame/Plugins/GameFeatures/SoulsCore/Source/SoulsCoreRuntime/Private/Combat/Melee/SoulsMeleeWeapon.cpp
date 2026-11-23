#include "Combat/Melee/SoulsMeleeWeapon.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsMeleeWeapon)

// 提供可以直接运行的细长方块武器，正式美术可在蓝图中替换。
ASoulsMeleeWeapon::ASoulsMeleeWeapon()
{
    bReplicates = true;
    WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
    SetRootComponent(WeaponRoot);
    BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
    BladeMesh->SetupAttachment(WeaponRoot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    BladeMesh->SetStaticMesh(Cube.Object);
    BladeMesh->SetRelativeLocation(FVector(57.5f, 0.0f, 0.0f));
    BladeMesh->SetRelativeScale3D(FVector(0.95f, 0.045f, 0.09f));
    BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BladeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BladeRoot"));
    BladeRoot->SetupAttachment(WeaponRoot);
    BladeRoot->SetRelativeLocation(FVector(10.0f, 0.0f, 0.0f));
    BladeTip = CreateDefaultSubobject<USceneComponent>(TEXT("BladeTip"));
    BladeTip->SetupAttachment(WeaponRoot);
    BladeTip->SetRelativeLocation(FVector(105.0f, 0.0f, 0.0f));
}

// 统一转换到武器根坐标，采样点调整不依赖网格 Socket。
void ASoulsMeleeWeapon::GetLocalBladePoints(FVector& OutRoot, FVector& OutTip) const
{
    const FTransform Transform = GetActorTransform();
    OutRoot = Transform.InverseTransformPosition(BladeRoot->GetComponentLocation());
    OutTip = Transform.InverseTransformPosition(BladeTip->GetComponentLocation());
}

// 供调试与测试观察当前刀刃的真实采样范围。
void ASoulsMeleeWeapon::GetBladePoints(FVector& OutRoot, FVector& OutTip) const
{
    OutRoot = BladeRoot->GetComponentLocation();
    OutTip = BladeTip->GetComponentLocation();
}
