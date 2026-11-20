#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacter.h"
#include "SoulsCharacter.generated.h"

class USoulsHeroComponent;
class USoulsLockOnComponent;
class USoulsMeleeTraceComponent;
class USoulsCombatReactionComponent;
class UAnimInstance;

/** 保留 Lyra 初始化与生命体系的魂类玩家角色。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsCharacter : public ALyraCharacter
{
    GENERATED_BODY()

public:
    // 创建魂类输入和锁定组件并设置自由移动参数。
    ASoulsCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
    // 为现有 Lyra 主动画蓝图链接无武器移动动画层。
    virtual void BeginPlay() override;

    // 占有后为没有队伍的 Souls 玩家分配默认阵营。
    virtual void PossessedBy(AController* NewController) override;

    UPROPERTY(EditDefaultsOnly, Category = "Souls|Animation")
    TSubclassOf<UAnimInstance> UnarmedAnimationLayer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls")
    TObjectPtr<USoulsHeroComponent> SoulsHero;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls")
    TObjectPtr<USoulsLockOnComponent> LockOn;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Combat")
    TObjectPtr<USoulsMeleeTraceComponent> MeleeTrace;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Combat")
    TObjectPtr<USoulsCombatReactionComponent> CombatReaction;

    UPROPERTY(EditDefaultsOnly, Category = "Souls|Combat", meta = (ClampMin = "0", ClampMax = "254"))
    uint8 DefaultPlayerTeamId = 1;
};
