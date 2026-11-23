#pragma once

#include "AIController.h"
#include "SoulsEnemyController.generated.h"

class ASoulsEnemy;
class UAIPawnData;
class USoulsAttackDefinition;
struct FSoulsBossAttackOption;

enum class ESoulsEnemyAttackResult : uint8 { Unavailable, Turning, Started };

/** 普通近战敌人的服务端行为树入口与敌情来源。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsEnemyController : public AAIController
{
    GENERATED_BODY()

public:
    // 普通敌人使用自身的 ASC，无需创建 PlayerState。
    ASoulsEnemyController();

    // 等 Pawn 的属性和能力就绪后启动行为树。
    void StartBehaviorFromPawnData();

    // 死亡或解除接管时停止行为树、近战和导航。
    void StopCombat();

    // 将可见性、活动范围及战斗状态同步到行为树黑板。
    void RefreshEnemyContext();

    // 锁定回位意图，直到敌人真正到达出生点。
    void RequestReturnHome();

    // 抵达出生点后恢复朝向，稍后才重新索敌。
    void FinishReturnHome();

    // 检查 ASC 的攻击状态，覆盖前摇、命中和后摇。
    bool IsAttacking() const;

    // 检查 ASC 的受击状态，用于暂停导航及攻击。
    bool IsHitReacting() const;

    // 再次确认目标并转向，到位后调用已有的轻攻击能力。
    ESoulsEnemyAttackResult TryMeleeAttack(float DeltaSeconds);

    // 判断 Boss 是否在出招期间免疫普通硬直。
    bool HasBossAttackArmor() const;

    // 按当前阶段确定目标位置是否已进入任意招式的起手距离。
    bool IsWithinAttackRange(double DistanceSquared) const;

    // 返回可用招式的最远距离，供追击任务设置接受半径。
    float GetAttackApproachRange() const;

    // 在攻击结束后开始冷却，启动失败时只短暂延迟重试。
    void ScheduleAttackRetry(bool bAttackStarted);

protected:
    // 接管时记录每个敌人的出生位置和朝向。
    virtual void OnPossess(APawn* InPawn) override;

    // 在 Pawn 被解除接管前清理决策和战斗动作。
    virtual void OnUnPossess() override;

private:
    // 从可见且存活的敌对玩家中选择离当前敌人最近的一个。
    APawn* FindTarget(const ASoulsEnemy& Enemy, const UAIPawnData& Data) const;

    // 验证阵营、生命和玩家控制权，避免追逐死亡或已换下的角色。
    bool IsHostilePlayer(const ASoulsEnemy& Enemy, const APawn* Candidate) const;

    // 清理本次交战的目标及搜索位置。
    void ClearTarget();

    // 加载并校验匹配当前出生资产的 Boss 招式。
    bool InitializeBossProfile(const UAIPawnData& Data);

    // 选出距离和阶段允许且尽量与上一招不同的招式。
    const FSoulsBossAttackOption* SelectBossAttack(double DistanceSquared) const;

    // 仅在上一招收尾完成且目标可见时推进半血阶段。
    void TryEnterBossPhaseTwo(ASoulsEnemy& Enemy, double Now);

    // 向敌人 ASC 提交一次带动作定义的攻击并确认实际开招。
    bool TryActivateBossAttack(ASoulsEnemy& Enemy, const FSoulsBossAttackOption& Option);

    TWeakObjectPtr<APawn> Target;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USoulsAttackDefinition>> LoadedBossDefinitions;

    FName LastBossAttackId;
    float ActiveBossPause = 0.0f;
    bool bUsesBossProfile = false;
    bool bBossPhaseTwo = false;
    bool bAwaitingBossAttackTaskFinish = false;
    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
    double LastSeenTime = -1.0;
    double NextAttackTime = 0.0;
    double NextAcquireTime = 0.0;
    bool bBehaviorStarted = false;
    bool bReturningHome = false;
};
