#include "Player/Input/SoulsHeroComponent.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Camera/SoulsCameraMode.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Player/SoulsPawnData.h"
#include "Combat/Actions/SoulsCombatActionComponent.h"
#include "Combat/Melee/SoulsMeleeTraceComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/Input/SoulsInputConfig.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
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
    if (BoundSubsystem.IsValid() && RollMapping)
    {
        BoundSubsystem->RemoveMappingContext(RollMapping);
    }
    BoundMoveAction.Reset();
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
    BoundMoveAction = Config->FindNativeInputActionForTag(LyraGameplayTags::InputTag_Move, false);
    if (const UInputAction* LightAttack = Config->FindAbilityInputActionForTag(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Souls.LightAttack")), false))
    {
        InputHandles.Add(Input->BindAction(LightAttack, ETriggerEvent::Started, this, &ThisClass::Input_LightAttack).GetHandle());
    }
    if (!RollMapping)
    {
        RollAction = NewObject<UInputAction>(this);
        RollAction->ValueType = EInputActionValueType::Boolean;
        RollMapping = NewObject<UInputMappingContext>(this);
        RollMapping->MapKey(RollAction, EKeys::LeftAlt);
        RollMapping->MapKey(RollAction, EKeys::Gamepad_FaceButton_Right);
    }
    Subsystem->AddMappingContext(RollMapping, 1);
    InputHandles.Add(Input->BindAction(RollAction, ETriggerEvent::Started, this, &ThisClass::Input_Roll).GetHandle());
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

// 轻击的 Started 绑定独占按键提交，避免 Triggered 再直接激活一次能力。
void USoulsHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
    if (InputTag != FGameplayTag::RequestGameplayTag(TEXT("InputTag.Souls.LightAttack")))
    {
        Super::Input_AbilityInputTagPressed(InputTag);
    }
}

// 翻滚及轻击中保留动作位移，忽略普通移动输入。
void USoulsHeroComponent::Input_Move(const FInputActionValue& InputActionValue)
{
    const USoulsCombatActionComponent* Combat = GetOwner()->FindComponentByClass<USoulsCombatActionComponent>();
    const ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
    const ULyraAbilitySystemComponent* ASC = Extension ? Extension->GetLyraAbilitySystemComponent() : nullptr;
    if ((!Combat || !Combat->IsRolling()) &&
        (!ASC || !ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.Attacking")))))
    {
        Super::Input_Move(InputActionValue);
    }
}

// 将已有轻击输入的按下边沿交给战斗组件。
void USoulsHeroComponent::Input_LightAttack(const FInputActionValue& Value)
{
    if (USoulsCombatActionComponent* Combat = GetOwner()->FindComponentByClass<USoulsCombatActionComponent>())
    {
        Combat->SubmitInput(ESoulsCombatInput::LightAttack);
    }
}

// 捕获当前移动轴在相机平面内的方向，零向量交由服务端选择角色前向。
void USoulsHeroComponent::Input_Roll(const FInputActionValue& Value)
{
    USoulsCombatActionComponent* Combat = GetOwner()->FindComponentByClass<USoulsCombatActionComponent>();
    const APlayerController* PC = GetController<APlayerController>();
    if (!Combat || !PC)
    {
        return;
    }
    FVector Direction = FVector::ZeroVector;
    const UEnhancedPlayerInput* EnhancedPlayerInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
    if (EnhancedPlayerInput && BoundMoveAction.IsValid())
    {
        if (const FInputActionInstance* Move = EnhancedPlayerInput->FindActionInstanceData(BoundMoveAction.Get()))
        {
            const FVector2D Axes = Move->GetValue().Get<FVector2D>();
            const FRotator CameraYaw(0.0f, PC->GetControlRotation().Yaw, 0.0f);
            Direction = CameraYaw.RotateVector(FVector(Axes.Y, Axes.X, 0.0f));
        }
    }
    Combat->SubmitInput(ESoulsCombatInput::Roll, Direction);
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
    const USoulsMeleeTraceComponent* Trace = GetOwner()->FindComponentByClass<USoulsMeleeTraceComponent>();
    const USoulsCombatActionComponent* Combat = GetOwner()->FindComponentByClass<USoulsCombatActionComponent>();
    if ((Trace && Trace->IsAttackActive()) || (Combat && Combat->IsRolling()))
    {
        bStickSwitchArmed = true;
        return;
    }
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
