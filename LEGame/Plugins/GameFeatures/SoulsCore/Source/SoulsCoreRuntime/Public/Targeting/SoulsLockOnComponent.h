#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "SoulsLockOnComponent.generated.h"

class USoulsTargetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSoulsLockTargetChanged, AActor*, TargetActor);

/** 管理本地玩家的目标选择、切换与失效规则。 */
UCLASS(ClassGroup = (Souls), meta = (BlueprintSpawnableComponent))
class SOULSCORERUNTIME_API USoulsLockOnComponent : public UPawnComponent
{
    GENERATED_BODY()

public:
    // 初始化仅在锁定期间运行的检查。
    USoulsLockOnComponent(const FObjectInitializer& ObjectInitializer);
    // 开启或取消锁定。
    UFUNCTION(BlueprintCallable, Category = "Souls|LockOn")
    void ToggleLockOn();
    // 根据屏幕左右方向切换目标。
    UFUNCTION(BlueprintCallable, Category = "Souls|LockOn")
    void SwitchTarget(float Direction);
    // 解除当前目标并发送状态事件。
    UFUNCTION(BlueprintCallable, Category = "Souls|LockOn")
    void ClearTarget();
    // 返回当前锁定点。
    USoulsTargetComponent* GetTarget() const;
    // 返回当前锁定对象。
    UFUNCTION(BlueprintPure, Category = "Souls|LockOn")
    AActor* GetTargetActor() const;

    UPROPERTY(BlueprintAssignable, Category = "Souls|LockOn")
    FSoulsLockTargetChanged OnTargetChanged;

    UPROPERTY(EditAnywhere, Category = "Souls|LockOn", meta = (ClampMin = "100"))
    float AcquireDistance = 1800.0f;
    UPROPERTY(EditAnywhere, Category = "Souls|LockOn", meta = (ClampMin = "1.0"))
    float RetainDistanceMultiplier = 1.25f;
    UPROPERTY(EditAnywhere, Category = "Souls|LockOn", meta = (ClampMin = "0", ClampMax = "89"))
    float AcquireHalfAngle = 70.0f;
    UPROPERTY(EditAnywhere, Category = "Souls|LockOn", meta = (ClampMin = "0"))
    float OcclusionGracePeriod = 0.6f;

protected:
    // 在锁定期间检查死亡、距离和遮挡。
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // 卸载时解除目标和事件绑定。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 判断目标距离、视野及遮挡是否满足获取条件。
    bool CanAcquire(const USoulsTargetComponent* Target, FVector2D& OutScreenPosition) const;
    // 检查玩家到目标间的视线是否被场景阻挡。
    bool HasLineOfSight(const USoulsTargetComponent* Target) const;
    // 切换目标并维护对象销毁监听。
    void SetTarget(USoulsTargetComponent* Target);
    // 目标离开世界时立即清除锁定。
    UFUNCTION()
    void HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

    TWeakObjectPtr<USoulsTargetComponent> CurrentTarget;
    float OccludedTime = 0.0f;
};
