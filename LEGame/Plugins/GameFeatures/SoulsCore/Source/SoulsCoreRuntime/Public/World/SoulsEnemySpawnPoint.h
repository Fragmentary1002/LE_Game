#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoulsEnemySpawnPoint.generated.h"

class UAIPawnData;

/** 在地图中标记一个带持久存档标识的敌人出生位置。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsEnemySpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    // 创建用于放置和调整朝向的出生锚点。
    ASoulsEnemySpawnPoint();

    // 返回序列化到地图中的出生点标识。
    const FGuid& GetSpawnId() const { return SpawnId; }

    // 返回传给现有敌人工厂的角色配置。
    UAIPawnData* GetPawnData() const { return PawnData.Get(); }

    // 返回该敌人是否需要将击杀写入永久进度。
    bool IsPermanentOnKill() const { return bPermanentOnKill; }

protected:
    // 锚点进入游戏世界时登记当前槽位。
    virtual void BeginPlay() override;

    // 先清理占用槽位的敌人，再结束锚点的生命周期。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    // 新放置的编辑器锚点获得初始标识。
    virtual void PostActorCreated() override;

    // 编辑器复制粘贴时为新锚点分配独立标识。
    virtual void PostEditImport() override;
#endif

private:
    UPROPERTY(VisibleInstanceOnly, Category = "Souls|Encounter")
    FGuid SpawnId;

    UPROPERTY(EditAnywhere, Category = "Souls|Encounter")
    TObjectPtr<UAIPawnData> PawnData;

    UPROPERTY(EditAnywhere, Category = "Souls|Encounter")
    bool bPermanentOnKill = false;
};
