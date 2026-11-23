#include "Enemies/SoulsEnemy.h"

#include "Enemies/AIPawnData.h"
#include "Components/PointLightComponent.h"
#include "TimerManager.h"
#include "Enemies/SoulsEnemyController.h"
#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/LyraAbilityTagRelationshipMapping.h"
#include "Combat/Melee/SoulsAttackDefinition.h"
#include "Combat/Melee/SoulsMeleeTraceComponent.h"
#include "Combat/Melee/SoulsMeleeWeapon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsEnemy)

// 复用训练角色的生命、受击与死亡流程，并补上伤害属性和武器轨迹。
ASoulsEnemy::ASoulsEnemy(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    AIControllerClass = ASoulsEnemyController::StaticClass();
    EnemyCombatSet = CreateDefaultSubobject<ULyraCombatSet>(TEXT("EnemyCombatSet"));
    MeleeTrace = CreateDefaultSubobject<USoulsMeleeTraceComponent>(TEXT("MeleeTrace"));
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
}

// 控制器使用同一份出生配置读取索敌与移动参数。
const UAIPawnData* ASoulsEnemy::GetAIPawnData() const
{
    return AIPawnData;
}

// 返回服务端战斗配置是否已完成初始化。
bool ASoulsEnemy::IsBehaviorReady() const
{
    return bBehaviorReady;
}

// 先注入配置再完成生成，使动态生成与地图放置走相同初始化流程。
ASoulsEnemy* ASoulsEnemy::SpawnFromAIPawnData(UObject* WorldContextObject, UAIPawnData* Data, const FTransform& SpawnTransform)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || World->GetNetMode() == NM_Client || !Data || !Data->PawnClass)
    {
        return nullptr;
    }
    ASoulsEnemy* Enemy = World->SpawnActorDeferred<ASoulsEnemy>(Data->PawnClass, SpawnTransform);
    if (Enemy)
    {
        Enemy->AIPawnData = Data;
        Enemy->FinishSpawning(SpawnTransform);
    }
    return Enemy;
}

// 先设置基础值和武器，再使用数据资产授予能力并启动 AI。
void ASoulsEnemy::BeginPlay()
{
    if (AIPawnData)
    {
        InitialHealth = AIPawnData->MaxHealth;
        TeamId = AIPawnData->EnemyTeamId;
        CombatAbilities.Reset();
        AdditionalAttributeSets.Reset();
        StartupEffects.Reset();
        MeleeTrace->DefaultAttackDefinition = AIPawnData->AttackDefinition;
        if (AIPawnData->WeaponClass)
        {
            MeleeTrace->WeaponClass = AIPawnData->WeaponClass;
        }
        GetCharacterMovement()->MaxWalkSpeed = AIPawnData->MoveSpeed;
    }
    else
    {
        MeleeTrace->WeaponClass = nullptr;
    }

    Super::BeginPlay();
    if (!HasAuthority())
    {
        return;
    }
    if (!AIPawnData)
    {
        UE_LOG(LogTemp, Warning, TEXT("SoulsEnemy %s has no AIPawnData and will remain idle."), *GetName());
        return;
    }

    ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
    ASC->AddAttributeSetSubobject(EnemyCombatSet.Get());
    ASC->SetNumericAttributeBase(ULyraCombatSet::GetBaseDamageAttribute(), FMath::Max(AIPawnData->BaseDamage, 0.0f));
    ASC->SetTagRelationshipMapping(AIPawnData->TagRelationshipMapping);
    SpawnDefaultController();
    ASC->RefreshAbilityActorInfo();
    for (const ULyraAbilitySet* AbilitySet : AIPawnData->AbilitySets)
    {
        if (AbilitySet)
        {
            AbilitySet->GiveToAbilitySystem(ASC, nullptr, this);
        }
    }

    bBehaviorReady = true;
    if (ASoulsEnemyController* EnemyController = Cast<ASoulsEnemyController>(GetController()))
    {
        EnemyController->StartBehaviorFromPawnData();
    }
}

// 从武器一侧发出短暂的红光，提示半血后开放新招式。
void ASoulsEnemy::PlayBossPhaseCue(float Duration)
{
    UPointLightComponent* Cue = NewObject<UPointLightComponent>(this);
    Cue->SetupAttachment(GetRootComponent());
    Cue->SetRelativeLocation(FVector(45.0, 0.0, 65.0));
    Cue->SetLightColor(FLinearColor(1.0f, 0.13f, 0.025f));
    Cue->SetIntensity(5000.0f);
    Cue->SetAttenuationRadius(600.0f);
    Cue->SetCastShadows(false);
    Cue->RegisterComponent();

    FTimerHandle CueTimer;
    // 提示结束后释放临时光源，组件销毁时自动撤销回调。
    GetWorldTimerManager().SetTimer(CueTimer, FTimerDelegate::CreateWeakLambda(Cue, [Cue]()
    {
        Cue->DestroyComponent();
    }), Duration, false);
}

// 与 Lyra 的死亡移动清理同步结束控制器的战斗状态。
void ASoulsEnemy::OnDeathStarted(AActor* OwningActor)
{
    if (ASoulsEnemyController* EnemyController = Cast<ASoulsEnemyController>(GetController()))
    {
        EnemyController->StopCombat();
    }
    Super::OnDeathStarted(OwningActor);
}
