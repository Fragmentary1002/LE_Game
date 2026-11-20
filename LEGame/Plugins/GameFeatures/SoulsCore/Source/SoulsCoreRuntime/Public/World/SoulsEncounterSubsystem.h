#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "SoulsEncounterSubsystem.generated.h"

class AController;
class ASoulsEnemySpawnPoint;

DECLARE_MULTICAST_DELEGATE_TwoParams(FSoulsEnemyDefeated, FGuid /* SpawnId */, bool /* bPermanent */);

/** 在游戏世界内管理已加载的敌人出生槽位以及本轮击杀状态。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsEncounterSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 用存档快照启动或刷新遭遇，同一轮调度前的多次调用采用最后一份快照。
    void ApplyReset(const TSet<FGuid>& PermanentKills);

    // 每个槽位在本轮仅广播一次有效死亡，奖励与存档由订阅方处理。
    FSoulsEnemyDefeated OnEnemyDefeated;

protected:
    // 将运行时遭遇状态限制在游戏和 PIE 世界。
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

    // 取消待处理工作并解除实例监听。
    virtual void Deinitialize() override;

private:
    friend class ASoulsEnemySpawnPoint;

    struct FSpawnSlot
    {
        TWeakObjectPtr<ASoulsEnemySpawnPoint> SpawnPoint;
        TWeakObjectPtr<AActor> Enemy;
        TWeakObjectPtr<AController> Controller;
    };

    // 登记流入的锚点，并拒绝无效或重复的标识。
    void RegisterSpawnPoint(ASoulsEnemySpawnPoint* SpawnPoint);

    // 撤销对应锚点的登记，并主动清理工厂生成的实例。
    void UnregisterSpawnPoint(ASoulsEnemySpawnPoint* SpawnPoint);

    // 判断当前世界是否仍能处理权威侧的生成与清理。
    bool CanUpdateWorld() const;

    // 将生成和刷新安排在锚点及死亡回调退出之后。
    void ScheduleUpdate();

    // 提交最新快照并依次清理旧实例和生成新实例。
    void ProcessPendingUpdates();

    // 按槽位状态决定是否调用现有敌人工厂。
    void TrySpawnEnemy(const FGuid& SpawnId);

    // 先解除登记和监听，再按需要销毁该槽位的实例。
    void ReleaseEnemy(FSpawnSlot& Slot, bool bDestroyPawn);

    // 处理尚未绑定或已经解绑的实例，停止战斗并销毁自有对象。
    void DestroyEnemyActors(AActor* Enemy, AController* Controller, bool bDestroyPawn) const;

    // 在有效敌人开始死亡时记录击杀并向外广播。
    UFUNCTION()
    void HandleEnemyDeathStarted(AActor* Enemy);

    // 实例离开世界时回收登记和控制器引用。
    UFUNCTION()
    void HandleEnemyEndPlay(AActor* Enemy, EEndPlayReason::Type EndPlayReason);

    TMap<FGuid, FSpawnSlot> Slots;
    TMap<TWeakObjectPtr<AActor>, FGuid, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<TWeakObjectPtr<AActor>, FGuid>> SpawnIdsByEnemy;
    TSet<FGuid> DefeatedIds;
    TSet<FGuid> PendingPermanentKills;
    TSet<FGuid> PendingSpawnIds;
    FTimerHandle UpdateTimer;
    bool bHasAppliedSnapshot = false;
    bool bResetPending = false;
    bool bClearingEnemies = false;
    bool bDeinitializing = false;
};
