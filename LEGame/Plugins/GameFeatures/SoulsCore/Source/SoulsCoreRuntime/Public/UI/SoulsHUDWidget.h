#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SoulsHUDWidget.generated.h"

class APawn;
class ULyraHealthComponent;
class USoulsLockOnComponent;
class UProgressBar;
class UTextBlock;

/** 通过真实生命事件刷新血条，并绘制当前锁定标记。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsHUDWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // 配置 HUD 的输入和命中测试行为。
    USoulsHUDWidget(const FObjectInitializer& ObjectInitializer);
    // HUD 激活时保持游戏输入。
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
    // 创建可直接使用的原生 HUD 控件树。
    virtual TSharedRef<SWidget> RebuildWidget() override;
    // 绑定玩家切换及当前角色数据。
    virtual void NativeConstruct() override;
    // 对称释放生命值和控制器事件。
    virtual void NativeDestruct() override;
    // 将当前锁定点投影到 HUD，绘制目标标记。
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    // 玩家控制的角色改变后重新订阅数据。
    UFUNCTION()
    void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
    // 处理玩家生命值及最大生命值变更。
    UFUNCTION()
    void HandlePlayerHealth(ULyraHealthComponent* Component, float OldValue, float NewValue, AActor* Instigator);
    // 处理当前目标生命值变更。
    UFUNCTION()
    void HandleTargetHealth(ULyraHealthComponent* Component, float OldValue, float NewValue, AActor* Instigator);
    // 切换目标血条数据来源。
    UFUNCTION()
    void HandleTargetChanged(AActor* Target);
    // 读取角色当前生命值并更新显示。
    void RefreshPlayerHealth();
    // 读取锁定目标当前生命值并更新显示。
    void RefreshTargetHealth();
    // 解绑当前玩家及目标的所有事件。
    void UnbindPawn();
    // 解绑当前目标生命事件。
    void UnbindTarget();

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> PlayerHealthBar;
    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PlayerHealthText;
    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> TargetHealthBar;
    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TargetText;
    TWeakObjectPtr<ULyraHealthComponent> PlayerHealth;
    TWeakObjectPtr<ULyraHealthComponent> TargetHealth;
    TWeakObjectPtr<USoulsLockOnComponent> LockOn;
};
