#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "SoulsTrainingTarget.generated.h"

class ULyraAbilitySystemComponent;
class ULyraHealthSet;
class ULyraHealthComponent;
class UStaticMeshComponent;
class USoulsTargetComponent;

/** 无 AI 的测试假人，提供锁定点和真实生命数据。 */
UCLASS()
class SOULSCORERUNTIME_API ASoulsTrainingTarget : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // 创建假人外观、能力系统、生命组件和锁定点。
    ASoulsTrainingTarget();
    // 返回假人的能力系统。
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    // 在游戏内通过伤害效果验证血条更新及死亡解除锁定。
    UFUNCTION(BlueprintCallable, Category = "Souls|Development")
    void ApplyTestDamage(float Amount = 25.0f);

protected:
    // 初始化能力系统并绑定生命数据。
    virtual void BeginPlay() override;
    // 退出时解除生命数据绑定。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls")
    TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls")
    TObjectPtr<USoulsTargetComponent> TargetPoint;
    UPROPERTY(VisibleAnywhere, Category = "Souls")
    TObjectPtr<ULyraAbilitySystemComponent> AbilitySystem;
    UPROPERTY()
    TObjectPtr<ULyraHealthSet> HealthSet;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls")
    TObjectPtr<ULyraHealthComponent> Health;
};
