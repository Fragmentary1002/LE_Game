#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoulsSoulDrop.generated.h"

class APawn;
class UStaticMeshComponent;

/** GameMode 根据持久掉魂记录生成的世界表现。 */
UCLASS(Blueprintable)
class SOULSCORERUNTIME_API ASoulsSoulDrop : public AActor
{
    GENERATED_BODY()

public:
    // 创建可由蓝图替换的掉魂外观。
    ASoulsSoulDrop();

    // 在权威端一次性绑定有效的掉魂记录 ID。
    void InitializeDrop(const FGuid& InDropId);

    // 返回该 Actor 对应的掉魂记录 ID。
    UFUNCTION(BlueprintPure, Category = "Souls|Progress")
    FGuid GetDropId() const;

    // 检查当前玩家 Pawn 是否满足本地回收条件，由 GameMode 核对持久记录并提交回收。
    bool CanUseBy(const APawn& Pawn) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Souls|Progress")
    TObjectPtr<UStaticMeshComponent> Visual;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Souls|Progress", meta = (ClampMin = "0.0", Units = "cm"))
    float UseDistance = 200.0f;

private:
    FGuid DropId;
};
