#include "Character/SoulsHeroComponent.h"

#include "Camera/SoulsCameraMode.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/SoulsPawnData.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/SoulsInputConfig.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "LyraGameplayTags.h"
#include "Targeting/SoulsLockOnComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsHeroComponent)

// 沿用 Lyra 的初始化状态与能力输入分发。
USoulsHeroComponent::USoulsHeroComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

// 在 Lyra 就绪流程启动前注册锁定状态监听。
void USoulsHeroComponent::BeginPlay()
{
    if (USoulsLockOnComponent* Lock = GetOwner()->FindComponentByClass<USoulsLockOnComponent>())
    {
        Lock->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
    }
    Super::BeginPlay();
}

// 仅移除本组件拥有的绑定和映射。
void USoulsHeroComponent::RemoveSoulsInput()
{
    if (UEnhancedInputComponent* Input = BoundInput.Get())
    {
        for (uint32 Handle : InputHandles)
        {
            Input->RemoveBindingByHandle(Handle);
        }
    }
    if (BoundSubsystem.IsValid() && BoundMapping.IsValid())
    {
        BoundSubsystem->RemoveMappingContext(BoundMapping.Get());
    }
    InputHandles.Reset();
    BoundInput.Reset();
    BoundSubsystem.Reset();
    BoundMapping.Reset();
}

// 在 Lyra 基础输入初始化后补充独立映射和锁定动作。
void USoulsHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
    RemoveSoulsInput();
    Super::InitializePlayerInput(PlayerInputComponent);
    const ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
    const ULyraPawnData* PawnData = Extension ? Extension->GetPawnData<ULyraPawnData>() : nullptr;
    const USoulsInputConfig* Config = PawnData ? Cast<USoulsInputConfig>(PawnData->InputConfig) : nullptr;
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    APlayerController* PC = GetController<APlayerController>();
    ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (!Config || !Input || !Subsystem || !Config->MappingContext)
    {
        return;
    }
    Subsystem->AddMappingContext(Config->MappingContext, 0);
    BoundInput = Input;
    BoundSubsystem = Subsystem;
    BoundMapping = Config->MappingContext;
    const UInputAction* LockAction = Config->FindNativeInputActionForTag(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Souls.LockOn")));
    const UInputAction* SwitchAction = Config->FindNativeInputActionForTag(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Souls.SwitchTarget")));
    if (LockAction)
    {
        InputHandles.Add(Input->BindAction(LockAction, ETriggerEvent::Started, this, &ThisClass::Input_ToggleLock).GetHandle());
    }
    if (SwitchAction)
    {
        InputHandles.Add(Input->BindAction(SwitchAction, ETriggerEvent::Started, this, &ThisClass::Input_SwitchTarget).GetHandle());
    }
    if (const UInputAction* Stick = Config->FindNativeInputActionForTag(LyraGameplayTags::InputTag_Look_Stick, false))
    {
        InputHandles.Add(Input->BindAction(Stick, ETriggerEvent::Completed, this, &ThisClass::Input_ResetStick).GetHandle());
        InputHandles.Add(Input->BindAction(Stick, ETriggerEvent::Canceled, this, &ThisClass::Input_ResetStick).GetHandle());
    }
}

// 锁定状态下忽略鼠标镜头旋转，避免与自动跟随争用控制旋转。
void USoulsHeroComponent::Input_LookMouse(const FInputActionValue& InputActionValue)
{
    const USoulsLockOnComponent* Lock = GetOwner()->FindComponentByClass<USoulsLockOnComponent>();
    if (!Lock || !Lock->GetTarget())
    {
        Super::Input_LookMouse(InputActionValue);
    }
}

// 摇杆拨动一次仅切换一次，回中后重新允许切换。
void USoulsHeroComponent::Input_LookStick(const FInputActionValue& InputActionValue)
{
    USoulsLockOnComponent* Lock = GetOwner()->FindComponentByClass<USoulsLockOnComponent>();
    if (!Lock || !Lock->GetTarget())
    {
        Super::Input_LookStick(InputActionValue);
        return;
    }
    const float Horizontal = InputActionValue.Get<FVector2D>().X;
    if (FMath::Abs(Horizontal) < 0.25f)
    {
        bStickSwitchArmed = true;
    }
    else if (FMath::Abs(Horizontal) > 0.65f && bStickSwitchArmed)
    {
        Lock->SwitchTarget(Horizontal);
        bStickSwitchArmed = false;
    }
}

// 摇杆释放时解除切换节流。
void USoulsHeroComponent::Input_ResetStick(const FInputActionValue& Value)
{
    bStickSwitchArmed = true;
}

// 通过目标组件切换锁定。
void USoulsHeroComponent::Input_ToggleLock(const FInputActionValue& Value)
{
    if (USoulsLockOnComponent* Lock = GetOwner()->FindComponentByClass<USoulsLockOnComponent>())
    {
        Lock->ToggleLockOn();
    }
}

// 将离散输入交给锁定组件处理。
void USoulsHeroComponent::Input_SwitchTarget(const FInputActionValue& Value)
{
    if (USoulsLockOnComponent* Lock = GetOwner()->FindComponentByClass<USoulsLockOnComponent>())
    {
        Lock->SwitchTarget(Value.Get<float>());
    }
}

// 锁定时使用控制器平滑转向，解除后恢复朝移动方向转向。
void USoulsHeroComponent::HandleTargetChanged(AActor* TargetActor)
{
    if (ACharacter* Character = GetPawn<ACharacter>())
    {
        Character->bUseControllerRotationYaw = false;
        UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        Movement->bOrientRotationToMovement = TargetActor == nullptr;
        Movement->bUseControllerDesiredRotation = TargetActor != nullptr;
    }
    bStickSwitchArmed = true;
}

// 技能相机具有最高优先级，常态相机由锁定状态决定。
TSubclassOf<ULyraCameraMode> USoulsHeroComponent::DetermineCameraMode() const
{
    if (AbilityCameraMode)
    {
        return AbilityCameraMode;
    }
    const USoulsLockOnComponent* Lock = GetOwner()->FindComponentByClass<USoulsLockOnComponent>();
    if (Lock && Lock->GetTarget())
    {
        const ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
        const USoulsPawnData* PawnData = Extension ? Extension->GetPawnData<USoulsPawnData>() : nullptr;
        if (PawnData && PawnData->LockOnCameraMode)
        {
            return PawnData->LockOnCameraMode;
        }
        return USoulsCameraMode_LockOn::StaticClass();
    }
    return Super::DetermineCameraMode();
}

// 卸载时释放输入和目标事件。
void USoulsHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveSoulsInput();
    if (USoulsLockOnComponent* Lock = GetOwner()->FindComponentByClass<USoulsLockOnComponent>())
    {
        Lock->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
    }
    Super::EndPlay(EndPlayReason);
}
