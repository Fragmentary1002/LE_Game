#pragma once

#include "CoreMinimal.h"
#include "Character/LyraHeroComponent.h"
#include "SoulsHeroComponent.generated.h"

class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;

/** 沿用 Lyra 初始化流程，为魂类角色补充锁定输入与相机选择。 */
UCLASS()
class SOULSCORERUNTIME_API USoulsHeroComponent : public ULyraHeroComponent
{
    GENERATED_BODY()

public:
    // 转交 Lyra 角色组件初始化参数。
    USoulsHeroComponent(const FObjectInitializer& ObjectInitializer);

protected:
    // 监听锁定变更以切换角色朝向策略。
    virtual void BeginPlay() override;
    // 解除输入映射和事件绑定。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // 保留 Lyra 基础绑定并添加魂类锁定输入。
    virtual void InitializePlayerInput(UInputComponent* PlayerInputComponent) override;
    // 自由模式转动镜头，锁定模式由相机决定朝向。
    virtual void Input_LookMouse(const FInputActionValue& InputActionValue) override;
    // 锁定时将右摇杆横向拨动转换为单次目标切换。
    virtual void Input_LookStick(const FInputActionValue& InputActionValue) override;
    // 优先保留能力相机，再选择自由或锁定相机。
    virtual TSubclassOf<ULyraCameraMode> DetermineCameraMode() const override;

private:
    // 切换锁定开关。
    void Input_ToggleLock(const FInputActionValue& Value);
    // 按输入方向切换目标。
    void Input_SwitchTarget(const FInputActionValue& Value);
    // 摇杆回中后允许下一次切换。
    void Input_ResetStick(const FInputActionValue& Value);
    // 依据目标状态切换角色旋转规则。
    UFUNCTION()
    void HandleTargetChanged(AActor* TargetActor);
    // 删除本组件添加的绑定和映射。
    void RemoveSoulsInput();

    TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundSubsystem;
    TWeakObjectPtr<UInputMappingContext> BoundMapping;
    TArray<uint32> InputHandles;
    bool bStickSwitchArmed = true;
};
