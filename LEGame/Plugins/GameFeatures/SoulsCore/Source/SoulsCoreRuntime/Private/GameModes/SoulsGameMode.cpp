#include "GameModes/SoulsGameMode.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnData.h"
#include "Character/SoulsCharacter.h"
#include "Checkpoint/SoulsBonfire.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/LyraPlayerStart.h"
#include "Player/LyraPlayerState.h"
#include "Progress/SoulsSoulDrop.h"
#include "World/SoulsEncounterSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsGameMode)

DEFINE_LOG_CATEGORY_STATIC(LogSoulsGameMode, Log, All);

namespace
{
    const FName DefaultStartTag(TEXT("SoulsDefaultStart"));
    constexpr int32 MaxRestartAttempts = 40;
    constexpr float RestartInterval = 0.25f;

    // 在不溢出的情况下发放非负魂奖励。
    int64 AddSouls(int64 Wallet, int64 Amount)
    {
        return Wallet + FMath::Clamp<int64>(Amount, 0, MAX_int64 - Wallet);
    }
}

// 在 Experience 加载完成后开始等待本地玩家与存档。
void ASoulsGameMode::OnExperienceLoaded(const ULyraExperienceDefinition*)
{
    bExperienceReady = true;
    CurrentMapId = FName(*UGameplayStatics::GetCurrentLevelName(this, true));
    QueuePlayerRestart(GetWorld()->GetFirstPlayerController());
}

// 覆盖登录入口，避免在 PlayerState 和进度服务就绪前生成角色。
void ASoulsGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    if (bExperienceReady)
    {
        QueuePlayerRestart(NewPlayer);
    }
}

// 首生与重生仅接受存档确认后的单机本地玩家。
bool ASoulsGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
    const ALyraPlayerState* State = Player ? Player->GetPlayerState<ALyraPlayerState>() : nullptr;
    return bWorldReady && !PendingCombatProgress.IsSet() && IsLocalSoulsPlayer(Player) && !Player->GetPawn()
        && State && State->GetPawnData<ULyraPawnData>() && Super::PlayerCanRestart_Implementation(Player);
}

// 优先使用已保存检查点，没有可用检查点时寻找地图的初始出生点。
AActor* ASoulsGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    const USoulsProgressSubsystem* Progress = GetProgressSubsystem();
    if (Progress)
    {
        const FSoulsCheckpoint& Checkpoint = Progress->GetProgress().LastCheckpoint;
        if (Checkpoint.MapId == CurrentMapId && Checkpoint.BonfireId.IsValid())
        {
            for (TActorIterator<ASoulsBonfire> It(GetWorld()); It; ++It)
            {
                ALyraPlayerStart* Start = It->GetCheckpointStart();
                if (It->GetBonfireId() == Checkpoint.BonfireId && IsValid(Start)
                    && Start->GetLocationOccupancy(Player) != ELyraPlayerStartLocationOccupancy::Full)
                {
                    return Start;
                }
            }
        }
    }
    for (TActorIterator<ALyraPlayerStart> It(GetWorld()); It; ++It)
    {
        if (It->PlayerStartTag == DefaultStartTag
            && It->GetLocationOccupancy(Player) != ELyraPlayerStartLocationOccupancy::Full)
        {
            return *It;
        }
    }
    return nullptr;
}

// 通知控制器本次生成失败，重试交由定时器处理。
void ASoulsGameMode::FailedToRestartPlayer(AController* NewPlayer)
{
    AGameModeBase::FailedToRestartPlayer(NewPlayer);
}

// 将玩法限制在当前运行中的单机本地玩家。
bool ASoulsGameMode::IsLocalSoulsPlayer(const APlayerController* Player) const
{
    return IsValid(Player) && Player->GetWorld() == GetWorld() && GetNetMode() == NM_Standalone
        && Player->IsLocalController();
}

// 获取与玩家当前游戏实例同生命周期的进度服务。
USoulsProgressSubsystem* ASoulsGameMode::GetProgressSubsystem() const
{
    UGameInstance* Instance = GetGameInstance();
    return Instance ? Instance->GetSubsystem<USoulsProgressSubsystem>() : nullptr;
}

// 为首生或死亡重生设置有界的重试窗口。
void ASoulsGameMode::QueuePlayerRestart(APlayerController* Player)
{
    if (IsLocalSoulsPlayer(Player))
    {
        PendingRestartPlayer = Player;
        RemainingRestartAttempts = MaxRestartAttempts;
        GetWorldTimerManager().SetTimer(RestartTimer, this, &ThisClass::TryPlayerRestart, RestartInterval, true, 0.01f);
    }
}

// 先恢复世界进度，再使用 Lyra 生成并占有新的玩家 Pawn。
void ASoulsGameMode::TryPlayerRestart()
{
    APlayerController* Player = PendingRestartPlayer.Get();
    if (!IsLocalSoulsPlayer(Player) || RemainingRestartAttempts-- <= 0)
    {
        if (IsValid(Player))
        {
            UE_LOG(LogSoulsGameMode, Error, TEXT("Player restart timed out: %s"), *GetNameSafe(Player));
        }
        PendingRestartPlayer.Reset();
        GetWorldTimerManager().ClearTimer(RestartTimer);
        return;
    }

    if (!bWorldReady)
    {
        USoulsProgressSubsystem* Progress = GetProgressSubsystem();
        USoulsEncounterSubsystem* Encounters = GetWorld()->GetSubsystem<USoulsEncounterSubsystem>();
        if (!bExperienceReady || !Progress || !Encounters || !Progress->LoadProgress())
        {
            return;
        }
        Encounters->OnEnemyDefeated.AddUObject(this, &ThisClass::HandleEnemyDefeated);
        Encounters->ApplyReset(Progress->GetProgress().PermanentKills);
        bWorldReady = true;
        SyncSoulDrop();
    }

    if (PlayerCanRestart(Player))
    {
        if (AActor* Start = ChoosePlayerStart(Player))
        {
            RestartPlayerAtPlayerStart(Player, Start);
        }
    }
    if (Player->GetPawn() && Player->GetPawn() != LastRecordedDeathPawn.Get())
    {
        if (bResetEncountersAfterDeath)
        {
            GetWorld()->GetSubsystem<USoulsEncounterSubsystem>()->ApplyReset(GetProgressSubsystem()->GetProgress().PermanentKills);
            bResetEncountersAfterDeath = false;
        }
        PendingRestartPlayer.Reset();
        GetWorldTimerManager().ClearTimer(RestartTimer);
    }
}

// 点亮和更换复活点需先存档，休息成功后补满生命并清理普通敌人的击杀状态。
bool ASoulsGameMode::RequestBonfire(APlayerController* Player, const ASoulsBonfire* Bonfire)
{
    if (!bWorldReady || PendingCombatProgress.IsSet() || !IsLocalSoulsPlayer(Player) || !IsValid(Bonfire))
    {
        return false;
    }
    ASoulsCharacter* Pawn = Cast<ASoulsCharacter>(Player->GetPawn());
    ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
    UAbilitySystemComponent* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
    ALyraPlayerStart* Start = Bonfire->GetCheckpointStart();
    const FGuid Id = Bonfire->GetBonfireId();
    if (!Id.IsValid() || !Pawn || !Health || !ASC || Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f
        || !Bonfire->CanUseBy(*Pawn) || !IsValid(Start) || Start->GetWorld() != GetWorld())
    {
        return false;
    }

    USoulsProgressSubsystem* Progress = GetProgressSubsystem();
    FSoulsProgressSnapshot Snapshot = Progress->GetProgress();
    const bool bAlreadyLit = Snapshot.UnlockedBonfires.Contains(Id);
    Snapshot.UnlockedBonfires.Add(Id);
    Snapshot.LastCheckpoint.MapId = CurrentMapId;
    Snapshot.LastCheckpoint.BonfireId = Id;
    if (!Progress->CommitProgress(Snapshot))
    {
        return false;
    }
    if (bAlreadyLit)
    {
        ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Health->GetMaxHealth());
        GetWorld()->GetSubsystem<USoulsEncounterSubsystem>()->ApplyReset(Snapshot.PermanentKills);
    }
    return true;
}

// 优先回收附近的掉魂，之后尝试最近且可用的篝火。
bool ASoulsGameMode::RequestInteraction(APlayerController* Player)
{
    if (!bWorldReady || PendingCombatProgress.IsSet() || !IsLocalSoulsPlayer(Player) || !Player->GetPawn())
    {
        return false;
    }
    const APawn& Pawn = *Player->GetPawn();
    if (ActiveDrop.IsValid() && ActiveDrop->CanUseBy(Pawn))
    {
        return TryRecoverSoulDrop(*Player);
    }
    ASoulsBonfire* Nearest = nullptr;
    double NearestDistance = TNumericLimits<double>::Max();
    for (TActorIterator<ASoulsBonfire> It(GetWorld()); It; ++It)
    {
        const double Distance = FVector::DistSquared(It->GetActorLocation(), Pawn.GetActorLocation());
        if (Distance < NearestDistance && It->CanUseBy(Pawn))
        {
            Nearest = *It;
            NearestDistance = Distance;
        }
    }
    return Nearest && RequestBonfire(Player, Nearest);
}

// 同一 Pawn 仅记一次死亡，即使钱包为空也替换上次掉魂。
void ASoulsGameMode::RecordPlayerDeath(APlayerController* Player, const FVector& SafeLocation)
{
    if (!bWorldReady || !IsLocalSoulsPlayer(Player) || !Player->GetPawn() || Player->GetPawn() == LastRecordedDeathPawn.Get())
    {
        return;
    }
    LastRecordedDeathPawn = Player->GetPawn();
    bResetEncountersAfterDeath = true;
    if (!PendingCombatProgress.IsSet())
    {
        PendingCombatProgress = GetProgressSubsystem()->GetProgress();
    }
    FSoulsProgressSnapshot& Snapshot = PendingCombatProgress.GetValue();
    Snapshot.SoulDrop = FSoulsDropRecord();
    if (Snapshot.WalletSouls > 0)
    {
        Snapshot.SoulDrop.DropId = FGuid::NewGuid();
        Snapshot.SoulDrop.MapId = CurrentMapId;
        Snapshot.SoulDrop.Location = SafeLocation + FVector(0, 0, 30);
        Snapshot.SoulDrop.Amount = Snapshot.WalletSouls;
    }
    Snapshot.WalletSouls = 0;
    QueuePlayerRestart(Player);
    FlushCombatProgress();
}

// 合并同帧击杀与可能尚未落盘的事件，永久敌人的奖励只记一次。
void ASoulsGameMode::HandleEnemyDefeated(FGuid SpawnId, bool bPermanent)
{
    const USoulsProgressSubsystem* Progress = GetProgressSubsystem();
    FSoulsProgressSnapshot Snapshot = PendingCombatProgress.Get(Progress->GetProgress());
    if (bPermanent && Snapshot.PermanentKills.Contains(SpawnId))
    {
        return;
    }
    if (bPermanent)
    {
        Snapshot.PermanentKills.Add(SpawnId);
    }
    Snapshot.WalletSouls = AddSouls(Snapshot.WalletSouls, EnemySoulReward);
    PendingCombatProgress = MoveTemp(Snapshot);
    FlushCombatProgress();
}

// 进度同步成功后发布掉魂表现，写盘故障恢复时重新排队重生。
void ASoulsGameMode::FlushCombatProgress()
{
    if (!PendingCombatProgress.IsSet())
    {
        return;
    }
    const bool bWasRetrying = GetWorldTimerManager().IsTimerActive(SaveRetryTimer);
    if (GetProgressSubsystem()->CommitProgress(PendingCombatProgress.GetValue()))
    {
        PendingCombatProgress.Reset();
        GetWorldTimerManager().ClearTimer(SaveRetryTimer);
        SyncSoulDrop();
        if (bWasRetrying)
        {
            APlayerController* Player = GetWorld()->GetFirstPlayerController();
            if (IsValid(Player) && (!Player->GetPawn() || Player->GetPawn() == LastRecordedDeathPawn.Get()))
            {
                QueuePlayerRestart(Player);
            }
        }
    }
    else if (!bWasRetrying)
    {
        UE_LOG(LogSoulsGameMode, Error, TEXT("Combat progress was not saved. Retrying while the level is open."));
        GetWorldTimerManager().SetTimer(SaveRetryTimer, this, &ThisClass::FlushCombatProgress, 2.0f, true);
    }
}

// 用当前地图和已确认的掉魂标识维护唯一可交互的掉魂 Actor。
void ASoulsGameMode::SyncSoulDrop()
{
    const FSoulsDropRecord& Drop = GetProgressSubsystem()->GetProgress().SoulDrop;
    const bool bShouldSpawn = Drop.DropId.IsValid() && Drop.MapId == CurrentMapId;
    if (ASoulsSoulDrop* Existing = ActiveDrop.Get())
    {
        if (bShouldSpawn && Existing->GetDropId() == Drop.DropId)
        {
            return;
        }
        Existing->Destroy();
        ActiveDrop.Reset();
    }
    if (bShouldSpawn)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        UClass* DropType = SoulDropClass ? SoulDropClass.Get() : ASoulsSoulDrop::StaticClass();
        if (ASoulsSoulDrop* Actor = GetWorld()->SpawnActor<ASoulsSoulDrop>(DropType, Drop.Location, FRotator::ZeroRotator, Params))
        {
            Actor->InitializeDrop(Drop.DropId);
            ActiveDrop = Actor;
        }
    }
}

// 以快照为准校验掉落，一次性回收后移除交互对象。
bool ASoulsGameMode::TryRecoverSoulDrop(APlayerController& Player)
{
    USoulsProgressSubsystem* Progress = GetProgressSubsystem();
    FSoulsProgressSnapshot Snapshot = Progress->GetProgress();
    const ASoulsSoulDrop* Drop = ActiveDrop.Get();
    if (!Drop || !Player.GetPawn() || !Drop->CanUseBy(*Player.GetPawn())
        || Snapshot.SoulDrop.MapId != CurrentMapId || Snapshot.SoulDrop.DropId != Drop->GetDropId())
    {
        return false;
    }
    Snapshot.WalletSouls = AddSouls(Snapshot.WalletSouls, Snapshot.SoulDrop.Amount);
    Snapshot.SoulDrop = FSoulsDropRecord();
    if (!Progress->CommitProgress(Snapshot))
    {
        return false;
    }
    SyncSoulDrop();
    return true;
}

// 界面只读取已确认的进度。
int64 ASoulsGameMode::GetWalletSouls() const
{
    const USoulsProgressSubsystem* Progress = GetProgressSubsystem();
    return Progress ? Progress->GetProgress().WalletSouls : 0;
}
