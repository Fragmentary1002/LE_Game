#pragma once

#include "CoreMinimal.h"
#include "Combat/Reactions/SoulsCombatDummy.h"
#include "SoulsEnemy.generated.h"

class UAIPawnData;
class ULyraCombatSet;
class USoulsMeleeTraceComponent;

/** 带独立 ASC、近战武器和服务端 AI 的普通敌人。 */
UCLASS(HideCategories = ("Souls|Combat Dummy"))
class SOULSCORERUNTIME_API ASoulsEnemy : public ASoulsCombatDummy
{
    GENERATED_BODY()

public:
    // 创建敌人的近战轨迹和伤害属性，并设置默认控制器。
    ASoulsEnemy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // 返回当前敌人的出生配置。
    const UAIPawnData* GetAIPawnData() const;

    // 仅在服务端属性与能力初始化后允许控制器启动行为树。
    bool IsBehaviorReady() const;

    // 按数据资产选择角色类，并在 BeginPlay 前注入配置。
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Souls|AI", meta = (WorldContext = "WorldContextObject"))
    static ASoulsEnemy* SpawnFromAIPawnData(UObject* WorldContextObject, UAIPawnData* Data, const FTransform& SpawnTransform);

    // 半血时给当前 Boss 播放一次短暂的红光提示。
    void PlayBossPhaseCue(float Duration);

protected:
    // 配置属性与武器并在服务端启动控制器。
    virtual void BeginPlay() override;

    // 死亡开始时立即清理寻路和追踪目标。
    virtual void OnDeathStarted(AActor* OwningActor) override;

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|AI", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAIPawnData> AIPawnData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Combat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USoulsMeleeTraceComponent> MeleeTrace;

    UPROPERTY()
    TObjectPtr<ULyraCombatSet> EnemyCombatSet;

    bool bBehaviorReady = false;
};
