#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoulsMeleeWeapon.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** 最小近战武器表现，两个独立采样点允许直接替换网格而不修改命中算法。 */
UCLASS(Blueprintable)
class SOULSCORERUNTIME_API ASoulsMeleeWeapon : public AActor
{
    GENERATED_BODY()

public:
    // 创建无碰撞占位刀刃与可编辑采样点。
    ASoulsMeleeWeapon();

    // 返回刀刃两端相对武器根节点的位置。
    void GetLocalBladePoints(FVector& OutRoot, FVector& OutTip) const;

    // 返回刀刃两端当前世界位置。
    UFUNCTION(BlueprintCallable, Category = "Souls|Weapon")
    void GetBladePoints(FVector& OutRoot, FVector& OutTip) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Weapon")
    TObjectPtr<USceneComponent> WeaponRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Weapon")
    TObjectPtr<UStaticMeshComponent> BladeMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Weapon")
    TObjectPtr<USceneComponent> BladeRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Weapon")
    TObjectPtr<USceneComponent> BladeTip;
};
