#include "Development/SoulsTrainingTarget.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameplayEffect.h"
#include "Targeting/SoulsTargetComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsTrainingTarget)

// 使用引擎圆柱体构建独立测试目标。
ASoulsTrainingTarget::ASoulsTrainingTarget()
{
    bReplicates = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
    SetRootComponent(Mesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Mesh->SetStaticMesh(Cylinder.Object);
    Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    TargetPoint = CreateDefaultSubobject<USoulsTargetComponent>(TEXT("TargetPoint"));
    TargetPoint->SetupAttachment(Mesh);
    TargetPoint->SetUsingAbsoluteScale(true);
    TargetPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
    AbilitySystem = CreateDefaultSubobject<ULyraAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    HealthSet = CreateDefaultSubobject<ULyraHealthSet>(TEXT("HealthSet"));
    Health = CreateDefaultSubobject<ULyraHealthComponent>(TEXT("Health"));
}

// 为锁定和伤害查询提供标准能力系统接口。
UAbilitySystemComponent* ASoulsTrainingTarget::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

// 将生命组件绑定到本 Actor 的能力系统。
void ASoulsTrainingTarget::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Health->InitializeWithAbilitySystem(AbilitySystem);
}

// 使用即时效果修改伤害元属性，经过正常生命事件链路。
void ASoulsTrainingTarget::ApplyTestDamage(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return;
    }
    UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
    Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = ULyraHealthSet::GetDamageAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(Amount);
    AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.0f, AbilitySystem->MakeEffectContext());
}

// 退出游戏时释放生命监听。
void ASoulsTrainingTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Health->UninitializeFromAbilitySystem();
    Super::EndPlay(EndPlayReason);
}
