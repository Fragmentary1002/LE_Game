#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "SoulsMeleeTraceComponent.generated.h"

class ASoulsMeleeWeapon;
class UAnimSequenceBase;
class USoulsAttackDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FSoulsMeleeHit, const FHitResult&);

/** 管理单次攻击的武器轨迹、命中去重以及临时移动和转向接管。 */
UCLASS(ClassGroup = (Souls), meta = (BlueprintSpawnableComponent))
class SOULSCORERUNTIME_API USoulsMeleeTraceComponent : public UPawnComponent
{
    GENERATED_BODY()

public:
    // 设置攻击期间才运行的姿势更新后采样。
    USoulsMeleeTraceComponent(const FObjectInitializer& ObjectInitializer);

    // 开始一次攻击，重置命中集合并接管普通移动和朝向。
    bool BeginAttack(const USoulsAttackDefinition* Definition = nullptr, float LungeDistance = 0.0f,
        bool bInUseAbilityHitWindow = false, const UAnimSequenceBase* Montage = nullptr);

    // 动画通知开启或关闭有效期，关闭不会清除本次命中集合。
    UFUNCTION(BlueprintCallable, Category = "Souls|Combat")
    void SetHitWindowEnabled(bool bEnabled);

    // 仅接收当前攻击 Montage 的窗口，突刺在开窗时开始位移。
    void SetMontageHitWindow(const UAnimSequenceBase* Animation, bool bEnabled, float NotifyDuration = 0.0f);

    // 停止采样并按当前锁定状态恢复移动，可重复调用。
    UFUNCTION(BlueprintCallable, Category = "Souls|Combat")
    void EndAttack();

    // 查询是否正在攻击，供输入与锁定状态变化避免覆盖临时朝向。
    UFUNCTION(BlueprintPure, Category = "Souls|Combat")
    bool IsAttackActive() const;

    // 查询当前动画命中窗口，供行为测试确认中断清理。
    UFUNCTION(BlueprintPure, Category = "Souls|Combat")
    bool IsHitWindowEnabled() const;

    // 返回本次攻击使用的配置。
    UFUNCTION(BlueprintPure, Category = "Souls|Combat")
    const USoulsAttackDefinition* GetAttackDefinition() const;

    // 返回当前实际生成的武器。
    UFUNCTION(BlueprintPure, Category = "Souls|Combat")
    ASoulsMeleeWeapon* GetWeapon() const;

    FSoulsMeleeHit OnHit;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Combat")
    TObjectPtr<USoulsAttackDefinition> DefaultAttackDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Combat")
    TSubclassOf<ASoulsMeleeWeapon> WeaponClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Combat")
    FName WeaponAttachSocket = TEXT("hand_r");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Combat")
    FTransform WeaponAttachTransform = FTransform(FRotator(0.0f, 0.0f, -90.0f));

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Souls|Combat")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel6;

protected:
    // 创建测试武器并使采样 Tick 依赖角色骨骼姿势更新。
    virtual void BeginPlay() override;

    // 在短起手期有限转向，并仅在有效期进行刀刃扫掠。
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 卸载时清理攻击与本组件创建的武器。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 对姿势间插值采样并沿刀刃多点做球体扫掠。
    void SampleWeaponTrajectory();

    // 对开窗时已经接触的物体执行初始重叠查询。
    void CheckInitialOverlap();

    // 过滤无效目标、死者、重复接触和环境遮挡后发送一次命中。
    void ProcessHit(const FHitResult& Hit);

    // 检查角色到武器和武器到接触点是否被环境阻挡。
    bool IsEnvironmentClear(const FVector& Start, const FVector& End, const AActor* Target) const;

    UPROPERTY(Transient)
    TObjectPtr<ASoulsMeleeWeapon> Weapon;

    // 移除本次突刺拥有的根运动源，保留其他移动效果。
    void StopLunge();

    TWeakObjectPtr<const USoulsAttackDefinition> ActiveAttackDefinition;
    TWeakObjectPtr<const UAnimSequenceBase> ActiveMontage;
    float ActiveLungeDistance = 0.0f;
    uint16 LungeSourceId = 0;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    FTransform PreviousWeaponTransform;
    FVector LocalBladeRoot = FVector::ZeroVector;
    FVector LocalBladeTip = FVector::ZeroVector;
    float AttackElapsed = 0.0f;
    float SavedMaxWalkSpeed = 0.0f;
    bool bAttackActive = false;
    bool bHitWindowEnabled = false;
    bool bUseAbilityHitWindow = false;
};
