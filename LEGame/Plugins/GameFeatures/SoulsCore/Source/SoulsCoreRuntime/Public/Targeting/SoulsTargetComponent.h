#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "SoulsTargetComponent.generated.h"

class USoulsTargetComponent;

/** 按世界登记目标，避免每帧遍历场景中的全部 Actor。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsTargetRegistry : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 返回当前世界登记的弱引用目标。
    const TArray<TWeakObjectPtr<USoulsTargetComponent>>& GetTargets() const { return Targets; }
    // 登记进入游戏的锁定目标。
    void RegisterTarget(USoulsTargetComponent* Target);
    // 移除离开游戏的锁定目标。
    void UnregisterTarget(USoulsTargetComponent* Target);

private:
    TArray<TWeakObjectPtr<USoulsTargetComponent>> Targets;
};

/** 显式标记可锁定对象，其位置即默认锁定点。 */
UCLASS(ClassGroup = (Souls), meta = (BlueprintSpawnableComponent))
class SOULSCORERUNTIME_API USoulsTargetComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    // 判断目标是否仍然可锁定，并排除已死亡对象。
    bool IsAvailable() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Souls|Target")
    bool bLockable = true;

protected:
    // 在世界中登记锁定点。
    virtual void BeginPlay() override;
    // 离开世界时撤销登记。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
