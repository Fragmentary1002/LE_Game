#include "World/SoulsEncounterSubsystem.h"

#include "AI/AIPawnData.h"
#include "AI/SoulsEnemy.h"
#include "AI/SoulsEnemyController.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/World.h"
#include "World/SoulsEnemySpawnPoint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsEncounterSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogSoulsEncounter, Log, All);

// 允许在真实游戏和 PIE 中使用独立的世界状态。
bool USoulsEncounterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// 关闭后取消待处理工作，并在世界回收 Actor 前解除监听。
void USoulsEncounterSubsystem::Deinitialize()
{
    bDeinitializing = true;
    if (UWorld* World = GetWorld(); World && UpdateTimer.IsValid())
    {
        World->GetTimerManager().ClearTimer(UpdateTimer);
    }

    TMap<FGuid, FSpawnSlot> RemovedSlots = MoveTemp(Slots);
    for (TPair<FGuid, FSpawnSlot>& Entry : RemovedSlots)
    {
        ReleaseEnemy(Entry.Value, true);
    }
    SpawnIdsByEnemy.Reset();
    DefeatedIds.Reset();
    PendingPermanentKills.Reset();
    PendingSpawnIds.Reset();
    OnEnemyDefeated.Clear();
    Super::Deinitialize();
}

// 接收最新永久击杀集合，并让待重置实例的死亡回调立即失效。
void USoulsEncounterSubsystem::ApplyReset(const TSet<FGuid>& PermanentKills)
{
    if (!CanUpdateWorld())
    {
        return;
    }

    PendingPermanentKills = PermanentKills;
    bResetPending = true;
    ScheduleUpdate();
}

// 登记当前加载的出生点，首次启动前保留槽位供协调器启用。
void USoulsEncounterSubsystem::RegisterSpawnPoint(ASoulsEnemySpawnPoint* SpawnPoint)
{
    if (!CanUpdateWorld())
    {
        return;
    }

    const FGuid& SpawnId = SpawnPoint->GetSpawnId();
    if (!SpawnId.IsValid())
    {
        UE_LOG(LogSoulsEncounter, Warning, TEXT("Spawn point %s has no persistent SpawnId."), *GetPathNameSafe(SpawnPoint));
        return;
    }
    if (const FSpawnSlot* ExistingSlot = Slots.Find(SpawnId))
    {
        if (ExistingSlot->SpawnPoint.Get() != SpawnPoint)
        {
            UE_LOG(LogSoulsEncounter, Error, TEXT("Duplicate SpawnId %s: %s and %s."),
                *SpawnId.ToString(), *GetPathNameSafe(ExistingSlot->SpawnPoint.Get()), *GetPathNameSafe(SpawnPoint));
        }
        return;
    }

    Slots.Add(SpawnId).SpawnPoint = SpawnPoint;
    if (bHasAppliedSnapshot)
    {
        PendingSpawnIds.Add(SpawnId);
        ScheduleUpdate();
    }
}

// 仅注销实际占用该槽位的锚点，避免重复标识的锚点误清理实例。
void USoulsEncounterSubsystem::UnregisterSpawnPoint(ASoulsEnemySpawnPoint* SpawnPoint)
{
    const FGuid& SpawnId = SpawnPoint->GetSpawnId();
    const FSpawnSlot* Slot = Slots.Find(SpawnId);
    if (!Slot || !Slot->SpawnPoint.HasSameIndexAndSerialNumber(TWeakObjectPtr<ASoulsEnemySpawnPoint>(SpawnPoint)))
    {
        return;
    }

    FSpawnSlot RemovedSlot;
    Slots.RemoveAndCopyValue(SpawnId, RemovedSlot);
    PendingSpawnIds.Remove(SpawnId);
    ReleaseEnemy(RemovedSlot, true);
}

// 只允许仍在运行的权威世界处理遭遇实例。
bool USoulsEncounterSubsystem::CanUpdateWorld() const
{
    const UWorld* World = GetWorld();
    return !bDeinitializing && World && !World->bIsTearingDown && World->GetNetMode() != NM_Client;
}

// 合并同一轮锚点注册和重置请求，避开发生在 Actor 回调栈内的生成。
void USoulsEncounterSubsystem::ScheduleUpdate()
{
    if (!UpdateTimer.IsValid())
    {
        UpdateTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ProcessPendingUpdates);
    }
}

// 重置时先移除本轮旧敌人，再按最新生效的存档快照生成。
void USoulsEncounterSubsystem::ProcessPendingUpdates()
{
    UpdateTimer.Invalidate();
    if (!CanUpdateWorld())
    {
        return;
    }

    if (bResetPending)
    {
        bResetPending = false;
        bHasAppliedSnapshot = true;
        DefeatedIds = MoveTemp(PendingPermanentKills);

        TGuardValue<bool> ClearingEnemies(bClearingEnemies, true);
        TArray<FGuid> LoadedSpawnIds;
        Slots.GetKeys(LoadedSpawnIds);
        for (const FGuid& SpawnId : LoadedSpawnIds)
        {
            if (FSpawnSlot* Slot = Slots.Find(SpawnId))
            {
                PendingSpawnIds.Add(SpawnId);
                ReleaseEnemy(*Slot, true);
            }
        }
    }

    TSet<FGuid> SpawnIdsToProcess = MoveTemp(PendingSpawnIds);
    PendingSpawnIds.Reset();
    for (const FGuid& SpawnId : SpawnIdsToProcess)
    {
        TrySpawnEnemy(SpawnId);
    }
}

// 校验槽位、生成敌人，随后绑定死亡和离场事件。
void USoulsEncounterSubsystem::TrySpawnEnemy(const FGuid& SpawnId)
{
    FSpawnSlot* Slot = Slots.Find(SpawnId);
    if (!CanUpdateWorld() || bResetPending || !Slot || Slot->Enemy.IsValid() || DefeatedIds.Contains(SpawnId))
    {
        return;
    }

    ASoulsEnemySpawnPoint* SpawnPoint = Slot->SpawnPoint.Get();
    if (!SpawnPoint)
    {
        return;
    }
    UAIPawnData* PawnData = SpawnPoint->GetPawnData();
    if (!PawnData || !PawnData->PawnClass)
    {
        UE_LOG(LogSoulsEncounter, Warning, TEXT("Spawn point %s needs PawnData with a PawnClass."), *GetPathNameSafe(SpawnPoint));
        return;
    }

    ASoulsEnemy* Enemy = ASoulsEnemy::SpawnFromAIPawnData(this, PawnData, SpawnPoint->GetActorTransform());
    if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed())
    {
        UE_LOG(LogSoulsEncounter, Warning, TEXT("Could not spawn enemy for SpawnId %s."), *SpawnId.ToString());
        return;
    }

    // 工厂会同步执行 BeginPlay，返回后重新检查锚点和世界状态。
    Slot = Slots.Find(SpawnId);
    if (!CanUpdateWorld() || bResetPending || !Slot || Slot->SpawnPoint.Get() != SpawnPoint || Slot->Enemy.IsValid())
    {
        DestroyEnemyActors(Enemy, Enemy->GetController(), true);
        return;
    }

    ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Enemy);
    if (!Health)
    {
        UE_LOG(LogSoulsEncounter, Error, TEXT("Enemy %s has no Lyra health component."), *GetPathNameSafe(Enemy));
        DestroyEnemyActors(Enemy, Enemy->GetController(), true);
        return;
    }

    Slot->Enemy = Enemy;
    Slot->Controller = Enemy->GetController();
    SpawnIdsByEnemy.Add(Slot->Enemy, SpawnId);
    Health->OnDeathStarted.AddDynamic(this, &ThisClass::HandleEnemyDeathStarted);
    Enemy->OnEndPlay.AddDynamic(this, &ThisClass::HandleEnemyEndPlay);
    if (Health->IsDeadOrDying())
    {
        HandleEnemyDeathStarted(Enemy);
    }
}

// 先从槽位移除实例，再停止战斗，避免清理过程计入击杀。
void USoulsEncounterSubsystem::ReleaseEnemy(FSpawnSlot& Slot, bool bDestroyPawn)
{
    AActor* Enemy = Slot.Enemy.Get();
    AController* Controller = Slot.Controller.Get();
    SpawnIdsByEnemy.Remove(Slot.Enemy);
    Slot.Enemy.Reset();
    Slot.Controller.Reset();

    if (Enemy)
    {
        if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Enemy))
        {
            Health->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleEnemyDeathStarted);
        }
        Enemy->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleEnemyEndPlay);
    }
    DestroyEnemyActors(Enemy, Controller, bDestroyPawn);
}

// 关闭自有控制器的战斗行为，再按需回收 Pawn 和剩余控制器。
void USoulsEncounterSubsystem::DestroyEnemyActors(AActor* Enemy, AController* Controller, bool bDestroyPawn) const
{
    const UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown || World->GetNetMode() == NM_Client)
    {
        return;
    }

    const bool bOwnsController = IsValid(Controller) && (!Controller->GetPawn() || Controller->GetPawn() == Enemy);
    if (bOwnsController)
    {
        if (ASoulsEnemyController* SoulsController = Cast<ASoulsEnemyController>(Controller))
        {
            SoulsController->StopCombat();
        }
    }
    if (bDestroyPawn && IsValid(Enemy) && !Enemy->IsActorBeingDestroyed())
    {
        Enemy->Destroy();
    }
    if (bOwnsController && IsValid(Controller) && (!Controller->GetPawn() || Controller->GetPawn() == Enemy))
    {
        Controller->Destroy();
    }
}

// 先写入当前击杀集合，再广播供奖励与存档订阅者处理。
void USoulsEncounterSubsystem::HandleEnemyDeathStarted(AActor* Enemy)
{
    if (!CanUpdateWorld() || bResetPending || bClearingEnemies || !IsValid(Enemy) || Enemy->IsActorBeingDestroyed())
    {
        return;
    }

    const FGuid* SpawnId = SpawnIdsByEnemy.Find(TWeakObjectPtr<AActor>(Enemy));
    if (!SpawnId || DefeatedIds.Contains(*SpawnId))
    {
        return;
    }
    const FSpawnSlot* Slot = Slots.Find(*SpawnId);
    const ASoulsEnemySpawnPoint* SpawnPoint = Slot ? Slot->SpawnPoint.Get() : nullptr;
    if (!SpawnPoint)
    {
        return;
    }

    const FGuid DefeatedId = *SpawnId;
    const bool bPermanent = SpawnPoint->IsPermanentOnKill();
    DefeatedIds.Add(DefeatedId);
    OnEnemyDefeated.Broadcast(DefeatedId, bPermanent);
}

// 自然死亡或外部销毁后的离场只清理实例，击杀状态由死亡事件决定。
void USoulsEncounterSubsystem::HandleEnemyEndPlay(AActor* Enemy, EEndPlayReason::Type)
{
    if (const FGuid* SpawnId = SpawnIdsByEnemy.Find(TWeakObjectPtr<AActor>(Enemy)))
    {
        if (FSpawnSlot* Slot = Slots.Find(*SpawnId))
        {
            ReleaseEnemy(*Slot, false);
        }
    }
}
