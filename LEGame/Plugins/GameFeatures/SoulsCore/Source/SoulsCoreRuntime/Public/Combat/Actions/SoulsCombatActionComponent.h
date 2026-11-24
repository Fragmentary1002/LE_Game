#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/PawnComponent.h"
#include "Engine/NetSerialization.h"
#include "TimerManager.h"
#include "GameplayEffect.h"
#include "LECombatCombo.h"
#include "SoulsCombatActionComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimMontage;

UENUM()
enum class ESoulsCombatInput : uint8
{
    LightAttack,
    Roll
};

/** 为单次翻滚提供有期限的 Lyra 伤害免疫。 */
UCLASS()
class USoulsRollImmunityEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    // 配置伤害免疫标签及其持续时长。
    USoulsRollImmunityEffect();
};

/** 负责服务端离散战斗输入、精力支出、动作接续及翻滚。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsCombatActionComponent : public UPawnComponent
{
    GENERATED_BODY()

public:
    // 初始化可复制的动作组件，空闲时不开启 Tick。
    USoulsCombatActionComponent(const FObjectInitializer& ObjectInitializer);

    // 复制翻滚状态，供拥有者限制普通移动和朝向。
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 由拥有者提交按键和按下时的移动方向。
    void SubmitInput(ESoulsCombatInput Input, const FVector& Direction = FVector::ZeroVector);

    // 判断当前精力和动作是否允许启动轻击。
    bool CanStartLightAttack() const;

    // 扣除本段精力并登记正在运行的轻击段。
    bool StartLightAttack(int32 Stage);

    // 结束本段并在自然完成时尝试接续。
    void FinishLightAttack(bool bCancelled);

    // 记录供派生规则使用的当前招式窗口。
    void SetComboWindow(FGameplayTag Window, bool bOpen);

    // 清除缓存和翻滚，用于实际受击、死亡及 ASC 解绑。
    void Interrupt();

    // 判断是否由翻滚持有移动和转向控制。
    bool IsRolling() const { return bRolling; }

protected:
    // 等待 Lyra 将当前 Pawn 绑定为 ASC 的 Avatar。
    virtual void BeginPlay() override;

    // 终止戏外回调并释放本 Pawn 持有的动作状态。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 推进翻滚窗口和精力恢复。
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(EditDefaultsOnly, Category = "Souls|Combo", meta = (RequiredAssetDataTags = "RowStructure=/Script/LEAbilityFramework.LECombatComboDefinitionRow"))
    TSoftObjectPtr<UDataTable> ComboDefinitions;

    UPROPERTY(EditDefaultsOnly, Category = "Souls|Combo", meta = (RequiredAssetDataTags = "RowStructure=/Script/LEAbilityFramework.LEAbilityRegistryRow"))
    TSoftObjectPtr<UDataTable> AbilityRegistry;

    UPROPERTY(Transient)
    TObjectPtr<const UDataTable> LoadedAbilityRegistry;

    UPROPERTY(Transient)
    TObjectPtr<const UDataTable> LoadedComboDefinitions;

    // 载入并校验连招定义和技能注册表。
    void LoadComboTables();

    // 按标签查找连招节点，并校验行名和节点标签一致。
    const FLECombatComboDefinitionRow* FindComboNode(FGameplayTag ComboTag) const;

    // 登记启动成功的节点，并为该节点授予临时标签。
    void EnterComboNode(FGameplayTag ComboTag);

    // 收回本节点授予的标签，并返回结束的节点标签。
    FGameplayTag LeaveComboNode();

    UPROPERTY(EditDefaultsOnly, Category = "Souls|Combo", meta = (ClampMin = "0"))
    float InputBufferSeconds = 0.18f;

    // 只接收拥有者发来的有效离散意图。
    UFUNCTION(Server, Reliable)
    void ServerSubmitInput(ESoulsCombatInput Input, FVector_NetQuantizeNormal Direction);

    // 依据当前动作立即派发或覆盖一条缓存意图。
    void AcceptInput(ESoulsCombatInput Input, const FVector& Direction);

    // 根据刚结束的动作选择下一招。
    void DispatchInput(const FLECombatIntent& Input, FGameplayTag SourceComboTag, const FGameplayTagContainer& Windows);

    // 等旧能力的任务清理后消费缓存。
    void DrainInput(FGameplayTag FinishedComboTag);

    // 预约下一帧处理新的按键，避免重入当前 Montage 回调。
    void ScheduleDrain(FGameplayTag FinishedComboTag);

    // 校验服务端当前 Pawn 的生存和输入状态。
    bool CanAct() const;

    // 检查精力并执行一次权威扣费。
    bool SpendStamina(float Cost);

    // 注册精力属性并为新生 Avatar 回满精力。
    void HandleAbilitySystemReady();

    // 在解绑时停止使用旧 ASC 和旧动作状态。
    void HandleAbilitySystemRemoved();

    // 在地面开启固定方向的翻滚位移。
    void StartRoll(const FVector& Direction, FGameplayTag ComboTag);

    // 释放位移与免疫资源，并根据结束结果决定是否接续。
    void FinishRoll(bool bCancelled);

    // 在白盒中播放与当前闪避时长匹配的占位动作。
    void PlayRouteRollMontage(ACharacter& Character);

    // 在状态变化时接管转向，结束时按当前锁定目标恢复转向。
    UFUNCTION()
    void OnRep_Rolling();

    TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
    TWeakObjectPtr<UAnimMontage> PlayingRollMontage;
    FLECombatCombo Combo;
    FGameplayTagContainer OpenComboWindows;
    FGameplayTagContainer BufferedComboWindows;
    FTimerHandle DrainTimer;
    FActiveGameplayEffectHandle ImmunityHandle;
    double LastSpendAt = 0.0;
    double RollStartedAt = 0.0;
    float PreRollMaxWalkSpeed = 0.0f;
    uint16 RollMotionId = 0;
    FGameplayTag PendingComboTag;
    FGameplayTag ActiveComboTag;
    FGameplayTagContainer ActiveGrantedTags;
    bool bImmunityWindowEntered = false;

    UPROPERTY(ReplicatedUsing = OnRep_Rolling)
    bool bRolling = false;
};
