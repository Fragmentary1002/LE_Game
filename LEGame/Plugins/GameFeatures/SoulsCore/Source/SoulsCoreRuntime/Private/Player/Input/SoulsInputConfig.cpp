#include "Player/Input/SoulsInputConfig.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "LyraGameplayTags.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsInputConfig)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Input_LockOn, "InputTag.Souls.LockOn");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Input_SwitchTarget, "InputTag.Souls.SwitchTarget");

// 创建基础动作，并在配置资产内部保存映射与修饰器。
USoulsInputConfig::USoulsInputConfig(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    UInputAction* Move = CreateDefaultSubobject<UInputAction>(TEXT("IA_Move"));
    Move->ValueType = EInputActionValueType::Axis2D;
    UInputAction* MouseLook = CreateDefaultSubobject<UInputAction>(TEXT("IA_LookMouse"));
    MouseLook->ValueType = EInputActionValueType::Axis2D;
    UInputAction* StickLook = CreateDefaultSubobject<UInputAction>(TEXT("IA_LookStick"));
    StickLook->ValueType = EInputActionValueType::Axis2D;
    UInputAction* Lock = CreateDefaultSubobject<UInputAction>(TEXT("IA_LockOn"));
    UInputAction* Switch = CreateDefaultSubobject<UInputAction>(TEXT("IA_SwitchTarget"));
    Switch->ValueType = EInputActionValueType::Axis1D;

    NativeInputActions = {
        { Move, LyraGameplayTags::InputTag_Move },
        { MouseLook, LyraGameplayTags::InputTag_Look_Mouse },
        { StickLook, LyraGameplayTags::InputTag_Look_Stick },
        { Lock, TAG_Input_LockOn },
        { Switch, TAG_Input_SwitchTarget }
    };

    MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Souls"));
    UInputModifierSwizzleAxis* ForwardAxis = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("ForwardAxis"));
    ForwardAxis->Order = EInputAxisSwizzle::YXZ;
    UInputModifierNegate* Negative = CreateDefaultSubobject<UInputModifierNegate>(TEXT("NegativeAxis"));
    UInputModifierNegate* InvertPitch = CreateDefaultSubobject<UInputModifierNegate>(TEXT("InvertPitch"));
    InvertPitch->bX = false;
    InvertPitch->bY = true;
    InvertPitch->bZ = false;
    UInputModifierDeadZone* DeadZone = CreateDefaultSubobject<UInputModifierDeadZone>(TEXT("StickDeadZone"));
    DeadZone->LowerThreshold = 0.2f;
    DeadZone->Type = EDeadZoneType::Radial;

    MappingContext->MapKey(Move, EKeys::W).Modifiers.Add(ForwardAxis);
    MappingContext->MapKey(Move, EKeys::S).Modifiers = { Negative, ForwardAxis };
    MappingContext->MapKey(Move, EKeys::A).Modifiers.Add(Negative);
    MappingContext->MapKey(Move, EKeys::D);
    MappingContext->MapKey(Move, EKeys::Gamepad_Left2D).Modifiers.Add(DeadZone);
    // UE 视口已经转换鼠标纵轴，直接传递给 Lyra 的俯仰输入。
    MappingContext->MapKey(MouseLook, EKeys::Mouse2D);
    MappingContext->MapKey(StickLook, EKeys::Gamepad_Right2D).Modifiers = { DeadZone, InvertPitch };
    MappingContext->MapKey(Lock, EKeys::MiddleMouseButton);
    MappingContext->MapKey(Lock, EKeys::Gamepad_RightThumbstick);
    MappingContext->MapKey(Switch, EKeys::Q).Modifiers.Add(Negative);
    MappingContext->MapKey(Switch, EKeys::E);
    MappingContext->MapKey(Switch, EKeys::MouseWheelAxis);
}
