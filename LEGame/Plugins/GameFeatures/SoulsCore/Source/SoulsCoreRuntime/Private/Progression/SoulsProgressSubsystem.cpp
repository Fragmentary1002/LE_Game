#include "Progression/SoulsProgressSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Templates/UnrealTemplate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsProgressSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogSoulsProgress, Log, All);

namespace
{
    constexpr uint32 SaveMagic = 0x4C554F53; // 按小端序写出 SOUL。
    constexpr int32 CurrentSchema = 1;
    constexpr int32 SaveUserIndex = 0;
    constexpr int32 MaxSaveBytes = 1024 * 1024;
    constexpr int32 MaxMapNameUnits = NAME_SIZE;
    constexpr int32 MaxProgressIds = 16384;
    constexpr int32 SlotCount = 2;
    const FString SlotNames[SlotCount] = {TEXT("SoulsProgress_A"), TEXT("SoulsProgress_B")};

    enum class ESlotState : uint8
    {
        Missing,
        Corrupt,
        UnsupportedSchema,
        Valid
    };

    struct FSlotProgress
    {
        FSoulsProgressSnapshot Snapshot;
        int64 Revision = 0;
        int32 Schema = 0;
    };

    // 验证快照的业务约束，未激活篝火时允许使用关卡默认出生点。
    bool IsValidSnapshot(const FSoulsProgressSnapshot& Snapshot)
    {
        if (Snapshot.WalletSouls < 0 ||
            (Snapshot.LastCheckpoint.BonfireId.IsValid() && Snapshot.LastCheckpoint.MapId.IsNone()))
        {
            return false;
        }

        const FSoulsDropRecord& Drop = Snapshot.SoulDrop;
        return !Drop.DropId.IsValid() || (!Drop.MapId.IsNone() && Drop.Amount > 0 && !Drop.Location.ContainsNaN());
    }

    // 用字符串保存地图名，避免将进程内的 FName 索引写入磁盘。
    void SerializeMapId(FArchive& Archive, FName& MapId)
    {
        FString Name = Archive.IsSaving() ? MapId.ToString() : FString();
        if (Name.Len() >= MaxMapNameUnits)
        {
            Archive.SetError();
            return;
        }

        if (Archive.IsLoading())
        {
            // FString 的长度字段为带符号的含终止符长度，先检查再让引擎分配缓冲区。
            const int64 StringOffset = Archive.Tell();
            int32 SerializedUnits = 0;
            Archive << SerializedUnits;
            if (Archive.IsError() || SerializedUnits == 0 ||
                SerializedUnits < -MaxMapNameUnits || SerializedUnits > MaxMapNameUnits)
            {
                Archive.SetError();
                return;
            }
            Archive.Seek(StringOffset);
        }

        Archive << Name;
        if (!Archive.IsError() && Archive.IsLoading())
        {
            if (Name.IsEmpty() || Name.Len() >= MaxMapNameUnits)
            {
                Archive.SetError();
                return;
            }
            MapId = FName(*Name);
        }
    }

    // 读写永久进度 ID 集合，并拒绝越界数量、重复 ID 和无效 ID。
    void SerializeIds(FArchive& Archive, TSet<FGuid>& Ids)
    {
        int32 Count = Ids.Num();
        Archive << Count;
        if (Archive.IsError() || Count < 0 || Count > MaxProgressIds)
        {
            Archive.SetError();
            return;
        }

        if (Archive.IsLoading())
        {
            constexpr int64 GuidBytes = 4 * sizeof(uint32);
            if (Count * GuidBytes > Archive.TotalSize() - Archive.Tell())
            {
                Archive.SetError();
                return;
            }

            Ids.Reset();
            Ids.Reserve(Count);
            for (int32 Index = 0; Index < Count; ++Index)
            {
                FGuid Id;
                Archive << Id;
                if (Archive.IsError() || !Id.IsValid() || Ids.Contains(Id))
                {
                    Archive.SetError();
                    return;
                }
                Ids.Add(Id);
            }
        }
        else
        {
            for (FGuid Id : Ids)
            {
                if (!Id.IsValid())
                {
                    Archive.SetError();
                    return;
                }
                Archive << Id;
            }
        }
    }

    // 按 Schema 1 的固定字段顺序读写进度，并以 DropId 决定是否附带掉魂数据。
    void SerializeSnapshot(FArchive& Archive, FSoulsProgressSnapshot& Snapshot)
    {
        SerializeMapId(Archive, Snapshot.LastCheckpoint.MapId);
        Archive << Snapshot.LastCheckpoint.BonfireId << Snapshot.WalletSouls;
        SerializeIds(Archive, Snapshot.UnlockedBonfires);
        SerializeIds(Archive, Snapshot.PermanentKills);
        Archive << Snapshot.SoulDrop.DropId;

        if (!Archive.IsError() && Snapshot.SoulDrop.DropId.IsValid())
        {
            FSoulsDropRecord& Drop = Snapshot.SoulDrop;
            SerializeMapId(Archive, Drop.MapId);
            Archive << Drop.Location.X << Drop.Location.Y << Drop.Location.Z << Drop.Amount;
        }
    }

    // 编码固定头、快照和尾部 CRC，校验覆盖版本号及全部业务字段。
    bool EncodeSlot(FSoulsProgressSnapshot& Snapshot, int64 Revision, TArray<uint8>& Bytes)
    {
        FMemoryWriter Writer(Bytes, /*bIsPersistent=*/true);
        uint32 Magic = SaveMagic;
        int32 Schema = CurrentSchema;
        Writer << Magic << Schema << Revision;
        SerializeSnapshot(Writer, Snapshot);
        if (Writer.IsError() || Bytes.Num() > MaxSaveBytes - static_cast<int32>(sizeof(uint32)))
        {
            return false;
        }

        uint32 Checksum = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
        Writer << Checksum;
        return !Writer.IsError();
    }

    // 先验证存档完整性，再识别版本并按对应格式读取快照。
    ESlotState DecodeSlot(const TArray<uint8>& Bytes, FSlotProgress& Progress)
    {
        constexpr int32 ChecksumBytes = sizeof(uint32);
        constexpr int32 HeaderBytes = sizeof(uint32) + sizeof(int32) + sizeof(int64);
        if (Bytes.Num() < HeaderBytes + ChecksumBytes || Bytes.Num() > MaxSaveBytes)
        {
            return ESlotState::Corrupt;
        }

        const int32 ContentSize = Bytes.Num() - ChecksumBytes;
        FMemoryReader Reader(Bytes, /*bIsPersistent=*/true);
        Reader.Seek(ContentSize);
        uint32 Checksum = 0;
        Reader << Checksum;
        if (Reader.IsError() || Checksum != FCrc::MemCrc32(Bytes.GetData(), ContentSize))
        {
            return ESlotState::Corrupt;
        }

        Reader.Seek(0);
        Reader.SetLimitSize(ContentSize);
        uint32 Magic = 0;
        Reader << Magic << Progress.Schema << Progress.Revision;
        if (Reader.IsError() || Magic != SaveMagic || Progress.Schema <= 0 || Progress.Revision <= 0)
        {
            return ESlotState::Corrupt;
        }
        if (Progress.Schema != CurrentSchema)
        {
            return ESlotState::UnsupportedSchema;
        }

        SerializeSnapshot(Reader, Progress.Snapshot);
        return !Reader.IsError() && Reader.Tell() == ContentSize && IsValidSnapshot(Progress.Snapshot)
            ? ESlotState::Valid : ESlotState::Corrupt;
    }

    // 读取一个槽，并区分无档、损坏和版本不兼容。
    ESlotState ReadSlot(int32 SlotIndex, FSlotProgress& Progress)
    {
        const FString& SlotName = SlotNames[SlotIndex];
        TArray<uint8> Bytes;
        if (UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, SaveUserIndex))
        {
            return DecodeSlot(Bytes, Progress);
        }
        return UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex) ? ESlotState::Corrupt : ESlotState::Missing;
    }
}

// 识别当前进程或世界是否来自编辑器预览，保持实档与测试数据隔离。
bool USoulsProgressSubsystem::UsesInMemoryStorage() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
    return !Context || Context->WorldType != EWorldType::Game || FParse::Param(FCommandLine::Get(), TEXT("PIEVIACONSOLE"));
}

// 初始化一次进度缓存，优先选择最新有效槽，并在槽损坏时回退到另一槽。
bool USoulsProgressSubsystem::LoadProgress()
{
    if (!IsInGameThread() || bStorageOperationInProgress)
    {
        return false;
    }
    if (bHasLoaded)
    {
        return true;
    }

    TGuardValue<bool> OperationGuard(bStorageOperationInProgress, true);
    bInMemoryStorage = UsesInMemoryStorage();
    if (bInMemoryStorage)
    {
        bHasLoaded = true;
        return true;
    }

    FSlotProgress Slots[SlotCount];
    int32 BestSlotIndex = INDEX_NONE;
    bool bHasCorruptSlot = false;
    bool bHasUnsupportedSlot = false;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const ESlotState State = ReadSlot(Index, Slots[Index]);
        if (State == ESlotState::Valid && (BestSlotIndex == INDEX_NONE || Slots[Index].Revision > Slots[BestSlotIndex].Revision))
        {
            BestSlotIndex = Index;
        }
        else if (State == ESlotState::Corrupt)
        {
            bHasCorruptSlot = true;
            UE_LOG(LogSoulsProgress, Warning, TEXT("Cannot read or validate progress in %s."), *SlotNames[Index]);
        }
        else if (State == ESlotState::UnsupportedSchema)
        {
            bHasUnsupportedSlot = true;
            UE_LOG(LogSoulsProgress, Error, TEXT("Unsupported progress schema %d in %s."), Slots[Index].Schema, *SlotNames[Index]);
        }
    }

    if (bHasUnsupportedSlot || (bHasCorruptSlot && BestSlotIndex == INDEX_NONE))
    {
        return false;
    }
    if (BestSlotIndex != INDEX_NONE)
    {
        CurrentProgress = MoveTemp(Slots[BestSlotIndex].Snapshot);
        Revision = Slots[BestSlotIndex].Revision;
        ActiveSlotIndex = BestSlotIndex;
        if (bHasCorruptSlot)
        {
            UE_LOG(LogSoulsProgress, Warning, TEXT("Recovered progress revision %lld from %s."), Revision, *SlotNames[BestSlotIndex]);
        }
    }

    bHasLoaded = true;
    return true;
}

// 返回子系统缓存的已确认快照，未成功加载时为初始空快照。
const FSoulsProgressSnapshot& USoulsProgressSubsystem::GetProgress() const
{
    check(IsInGameThread());
    return CurrentProgress;
}

// 交替写入备用槽，确认回读完全一致后再推进活跃槽、版本与快照。
bool USoulsProgressSubsystem::CommitProgress(const FSoulsProgressSnapshot& Snapshot)
{
    if (!IsInGameThread() || bStorageOperationInProgress || !IsValidSnapshot(Snapshot) ||
        !LoadProgress() || Revision == MAX_int64)
    {
        return false;
    }

    TGuardValue<bool> OperationGuard(bStorageOperationInProgress, true);
    FSoulsProgressSnapshot PendingProgress = Snapshot;
    if (!PendingProgress.SoulDrop.DropId.IsValid())
    {
        PendingProgress.SoulDrop = FSoulsDropRecord();
    }

    const int64 NextRevision = Revision + 1;
    TArray<uint8> Encoded;
    if (!EncodeSlot(PendingProgress, NextRevision, Encoded))
    {
        UE_LOG(LogSoulsProgress, Error, TEXT("Could not encode progress revision %lld."), NextRevision);
        return false;
    }

    if (!bInMemoryStorage)
    {
        const int32 WriteSlotIndex = ActiveSlotIndex == 0 ? 1 : 0;
        const FString& SlotName = SlotNames[WriteSlotIndex];
        TArray<uint8> ReadBack;
        if (!UGameplayStatics::SaveDataToSlot(Encoded, SlotName, SaveUserIndex) ||
            !UGameplayStatics::LoadDataFromSlot(ReadBack, SlotName, SaveUserIndex) || ReadBack != Encoded)
        {
            UE_LOG(LogSoulsProgress, Error, TEXT("Write or read-back verification failed for %s at revision %lld."), *SlotName, NextRevision);
            return false;
        }
        ActiveSlotIndex = WriteSlotIndex;
    }

    CurrentProgress = MoveTemp(PendingProgress);
    Revision = NextRevision;
    return true;
}
