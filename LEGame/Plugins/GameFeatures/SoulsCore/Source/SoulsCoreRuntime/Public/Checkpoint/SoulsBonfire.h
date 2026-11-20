#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoulsBonfire.generated.h"

class ALyraPlayerStart;
class APawn;
class APlayerController;

/** 关卡中的篝火交互目标，提供稳定标识和可配置的重生点。 */
UCLASS(Blueprintable)
class SOULSCORERUNTIME_API ASoulsBonfire : public AActor
{
    GENERATED_BODY()

public:
    // 创建用作交互位置的场景根组件。
    ASoulsBonfire();

    // 返回关卡实例的持久化篝火标识。
    FGuid GetBonfireId() const;

    // 返回为该篝火配置的 Lyra 重生点。
    ALyraPlayerStart* GetCheckpointStart() const;

    // 检查 Pawn 是否与篝火同世界、在交互距离内且视线无遮挡。
    bool CanUseBy(const APawn& Pawn) const;

    // 验证玩家的交互请求，并返回 GameMode 是否接受请求。
    UFUNCTION(BlueprintCallable, Category = "Souls|Bonfire")
    bool Interact(APlayerController* Player);

private:
#if WITH_EDITOR
    // 为新放置且未配置标识的篝火生成标识。
    virtual void PostActorCreated() override;

    // 为编辑器中复制出的关卡实例重新生成标识，保留 PIE 实例的原标识。
    virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

    // 为通过编辑器粘贴或导入创建的关卡实例重新生成标识。
    virtual void PostEditImport() override;

    // 判断当前实例是否是可保存的编辑器关卡 Actor。
    bool IsPlacedEditorInstance() const;
#endif

    UPROPERTY(EditInstanceOnly, Category = "Souls|Bonfire")
    FGuid BonfireId;

    UPROPERTY(EditInstanceOnly, Category = "Souls|Bonfire")
    TObjectPtr<ALyraPlayerStart> CheckpointStart = nullptr;

    UPROPERTY(EditAnywhere, Category = "Souls|Bonfire", meta = (ClampMin = "0.0", Units = "cm"))
    float InteractionDistance = 250.0f;
};
