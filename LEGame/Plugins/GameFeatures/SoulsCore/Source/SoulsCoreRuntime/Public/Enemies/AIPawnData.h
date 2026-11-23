#pragma once

#include "Engine/DataAsset.h"
#include "AIPawnData.generated.h"

class ASoulsEnemy;
class ASoulsMeleeWeapon;
class UBehaviorTree;
class ULyraAbilitySet;
class ULyraAbilityTagRelationshipMapping;
class USoulsAttackDefinition;

/** 普通敌人的出生配置。当前生命等运行时属性由每只敌人自己的 ASC 管理。 */
UCLASS(BlueprintType, Const)
class SOULSCORERUNTIME_API UAIPawnData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Pawn")
    TSubclassOf<ASoulsEnemy> PawnClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Abilities")
    TArray<TObjectPtr<ULyraAbilitySet>> AbilitySets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Abilities")
    TObjectPtr<ULyraAbilityTagRelationshipMapping> TagRelationshipMapping;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat", meta = (ClampMin = "1"))
    float MaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat", meta = (ClampMin = "0"))
    float BaseDamage = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat", meta = (ClampMin = "0", ClampMax = "254"))
    int32 EnemyTeamId = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat")
    TObjectPtr<USoulsAttackDefinition> AttackDefinition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat")
    TSubclassOf<ASoulsMeleeWeapon> WeaponClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|AI", meta = (ClampMin = "1", Units = "cm"))
    float AcquireRadius = 850.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|AI")
    TObjectPtr<UBehaviorTree> BehaviorTree;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|AI", meta = (ClampMin = "1", Units = "cm"))
    float LeashRadius = 1500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|AI", meta = (ClampMin = "0", Units = "s"))
    float SightGracePeriod = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|AI", meta = (ClampMin = "1", Units = "cm"))
    float AttackRange = 170.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|AI", meta = (ClampMin = "0", Units = "s"))
    float AttackCooldown = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|AI", meta = (ClampMin = "1"))
    float MoveSpeed = 300.0f;
};
