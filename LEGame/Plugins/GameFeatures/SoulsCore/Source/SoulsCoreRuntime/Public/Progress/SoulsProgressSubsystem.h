#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SoulsProgressSubsystem.generated.h"

/** 保存最后确认的复活地图及篝火稳定 ID。 */
USTRUCT()
struct SOULSCORERUNTIME_API FSoulsCheckpoint
{
    GENERATED_BODY()

    UPROPERTY()
    FName MapId = NAME_None;

    UPROPERTY()
    FGuid BonfireId;
};

/** 保存唯一的掉魂记录，无效 DropId 表示没有掉落。 */
USTRUCT()
struct SOULSCORERUNTIME_API FSoulsDropRecord
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid DropId;

    UPROPERTY()
    FName MapId = NAME_None;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    int64 Amount = 0;
};

/** 将复活点、永久进度、钱包和掉魂记录作为一个整体读写。 */
USTRUCT()
struct SOULSCORERUNTIME_API FSoulsProgressSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    FSoulsCheckpoint LastCheckpoint;

    UPROPERTY()
    TSet<FGuid> UnlockedBonfires;

    UPROPERTY()
    TSet<FGuid> PermanentKills;

    UPROPERTY()
    int64 WalletSouls = 0;

    UPROPERTY()
    FSoulsDropRecord SoulDrop;
};

/** 在游戏线程同步读写单机进度，并在切图时保留已确认的快照。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsProgressSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 同步加载一次可恢复的进度，无档时初始化空快照，失败后可重试。
    bool LoadProgress();

    // 在游戏线程读取最近一次成功加载或提交的快照。
    const FSoulsProgressSnapshot& GetProgress() const;

    // 同步提交整份快照，成功确认后更新缓存，失败时保留原快照。
    bool CommitProgress(const FSoulsProgressSnapshot& Snapshot);

private:
    // 根据所属 GameInstance 的世界上下文选择 PIE 内存存储或运行时磁盘存储。
    bool UsesInMemoryStorage() const;

    FSoulsProgressSnapshot CurrentProgress;
    int64 Revision = 0;
    int32 ActiveSlotIndex = INDEX_NONE;
    bool bHasLoaded = false;
    bool bInMemoryStorage = false;
    bool bStorageOperationInProgress = false;
};
