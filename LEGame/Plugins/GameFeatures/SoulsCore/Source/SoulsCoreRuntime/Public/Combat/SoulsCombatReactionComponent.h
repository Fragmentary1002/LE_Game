#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulsCombatReactionComponent.generated.h"

class ULyraAbilitySystemComponent;
class ULyraHealthSet;
struct FGameplayEffectSpec;

/** 将真实生命结算转换为带命中上下文的受击事件。 */
UCLASS(ClassGroup = (Souls), meta = (BlueprintSpawnableComponent))
class SOULSCORERUNTIME_API USoulsCombatReactionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 将反应模块绑定到当前角色的能力系统，可重复调用。
    void InitializeWithAbilitySystem(ULyraAbilitySystemComponent* InASC);
    // 卸载角色或能力系统时移除所有生命事件监听。
    void UninitializeFromAbilitySystem();
    // 返回是否已经绑定有效的生命集，便于配置诊断。
    UFUNCTION(BlueprintPure, Category = "Souls|Combat")
    bool IsInitialized() const;

protected:
    // 自动跟随 Lyra PawnExtension 的能力系统初始化流程。
    virtual void BeginPlay() override;
    // 结束游戏时清理生命事件监听。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 从 PawnExtension 获取刚刚就绪的能力系统。
    void HandleAbilitySystemInitialized();
    // 仅在实际掉血且仍存活时发送受击事件。
    void HandleHealthChanged(AActor* Instigator, AActor* Causer, const FGameplayEffectSpec* Spec,
        float Magnitude, float OldHealth, float NewHealth);

    TWeakObjectPtr<ULyraAbilitySystemComponent> BoundASC;
    TWeakObjectPtr<const ULyraHealthSet> BoundHealthSet;
};
