#pragma once

#include "CoreMinimal.h"
#include "GameModes/LyraGameMode.h"
#include "Progress/SoulsProgressSubsystem.h"
#include "TimerManager.h"
#include "SoulsGameMode.generated.h"

class ALyraPlayerStart;
class ASoulsBonfire;
class ASoulsSoulDrop;

/** 串联单机魂类玩法的进度提交、世界表现和 Lyra 重生。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsGameMode : public ALyraGameMode
{
    GENERATED_BODY()

public:
    // 按需点亮篝火，已点亮时执行休息。
    bool RequestBonfire(APlayerController* Player, const ASoulsBonfire* Bonfire);

    // 回收可交互的掉魂或使用最近的可交互篝火。
    bool RequestInteraction(APlayerController* Player);

    // 将死亡扣款和替换掉魂记录一起提交，再排队等候旧 Pawn 解绑。
    void RecordPlayerDeath(APlayerController* Player, const FVector& SafeLocation);

    // 读取已确认的持有魂数供界面使用。
    UFUNCTION(BlueprintPure, Category = "Souls|Progress")
    int64 GetWalletSouls() const;

protected:
    // 首次生成前接入已加载的存档和关卡。
    virtual void OnExperienceLoaded(const ULyraExperienceDefinition* CurrentExperience) override;

    // 晚加入的本地控制器复用相同的生成条件。
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

    // 防止外部重生请求绕过读档、死亡提交及 PawnData 的就绪条件。
    virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

    // 选择检查点的出生点，缺失或被占用时回退到地图起点。
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    // 不启用 Lyra 的逐帧重试，由本模式统一控制重试次数。
    virtual void FailedToRestartPlayer(AController* NewPlayer) override;

private:
    // 校验当前请求是否来自单机局内的本地玩家。
    bool IsLocalSoulsPlayer(const APlayerController* Player) const;

    // 返回当前 GameInstance 的进度服务。
    USoulsProgressSubsystem* GetProgressSubsystem() const;

    // 延后启动，确保 Experience 完成广播后再读取 PlayerState 的 PawnData。
    void QueuePlayerRestart(APlayerController* Player);

    // 在有界窗口内等待存档、出生点及角色就绪。
    void TryPlayerRestart();

    // 写入必须保留的战斗进度，失败时保留待提交快照并定时重试。
    void FlushCombatProgress();

    // 把击杀奖励及其永久标记写入同一份进度。
    void HandleEnemyDefeated(FGuid SpawnId, bool bPermanent);

    // 用当前已确认的记录更新掉魂 Actor。
    void SyncSoulDrop();

    // 校验掉落身份和距离后，一次提交完成加魂和清除记录。
    bool TryRecoverSoulDrop(APlayerController& Player);

    UPROPERTY(EditDefaultsOnly, Category = "Souls|Progress", meta = (ClampMin = "0"))
    int64 EnemySoulReward = 100;

    UPROPERTY(EditDefaultsOnly, Category = "Souls|Progress")
    TSubclassOf<ASoulsSoulDrop> SoulDropClass;

    TWeakObjectPtr<APlayerController> PendingRestartPlayer;
    TWeakObjectPtr<APawn> LastRecordedDeathPawn;
    TWeakObjectPtr<ASoulsSoulDrop> ActiveDrop;
    TOptional<FSoulsProgressSnapshot> PendingCombatProgress;
    FTimerHandle RestartTimer;
    FTimerHandle SaveRetryTimer;
    FName CurrentMapId = NAME_None;
    int32 RemainingRestartAttempts = 0;
    bool bExperienceReady = false;
    bool bWorldReady = false;
    bool bResetEncountersAfterDeath = false;
};
