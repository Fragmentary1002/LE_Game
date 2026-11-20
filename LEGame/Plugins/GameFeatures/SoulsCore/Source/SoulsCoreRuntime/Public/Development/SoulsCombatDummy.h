#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacter.h"
#include "SoulsCombatDummy.generated.h"

class UAttributeSet;
class UAnimInstance;
class UGameplayEffect;
class ULyraAbilitySystemComponent;
class ULyraGameplayAbility;
class ULyraHealthSet;
class USoulsCombatReactionComponent;
class USoulsTargetComponent;

/** 无行为树的骨骼训练敌人，使用独立 ASC 运行受击与死亡能力。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsCombatDummy : public ALyraCharacter
{
    GENERATED_BODY()

public:
    // 创建独立能力系统、生命集、受击反应和锁定点。
    ASoulsCombatDummy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    // 返回训练敌人的独立能力系统，而非 PlayerState 能力系统。
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    // 初始化生命、阵营和可配置能力。
    virtual void BeginPlay() override;
    // 退出时取消能力并解除生命与反应监听。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Combat Dummy", meta = (ClampMin = "1.0"))
    float InitialHealth = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Combat Dummy", meta = (ClampMin = "0", ClampMax = "254"))
    int32 TeamId = 2;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat Dummy")
    TArray<TSubclassOf<ULyraGameplayAbility>> CombatAbilities;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat Dummy")
    TArray<TSubclassOf<UAttributeSet>> AdditionalAttributeSets;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat Dummy")
    TArray<TSubclassOf<UGameplayEffect>> StartupEffects;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Combat Dummy")
    TSubclassOf<UAnimInstance> UnarmedAnimationLayer;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Combat Dummy")
    TObjectPtr<USoulsCombatReactionComponent> CombatReaction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Combat Dummy")
    TObjectPtr<USoulsTargetComponent> TargetPoint;

private:
    UPROPERTY(VisibleAnywhere, Category = "Souls|Combat Dummy")
    TObjectPtr<ULyraAbilitySystemComponent> CombatAbilitySystem;
    UPROPERTY()
    TObjectPtr<ULyraHealthSet> CombatHealthSet;
};
