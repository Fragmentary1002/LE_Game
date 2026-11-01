#include "LyraUIControlBinding.h"

#include "Components/Button.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIControlBinding)

/** 注册可移除的 Button 点击委托。 */
bool ULyraUIControlBinding::BindButton(UButton* Button, UObject* Target, FName FunctionName)
{
    if (!IsValid(Button) || !IsValid(Target) || FunctionName.IsNone() || !Target->FindFunction(FunctionName))
    {
        return false;
    }

    if (Target->FindFunction(FunctionName)->NumParms != 0)
    {
        return false;
    }

    FScriptDelegate Delegate;
    Delegate.BindUFunction(Target, FunctionName);

    for (int32 Index = 0; Index < BoundButtons.Num(); ++Index)
    {
        if (BoundButtons[Index].Get() == Button && BoundTargets[Index].Get() == Target && BoundFunctionNames[Index] == FunctionName)
        {
            return true;
        }
    }

    if (Button->OnClicked.Contains(Delegate))
    {
        return true;
    }

    Button->OnClicked.Add(Delegate);
    BoundButtons.Add(Button);
    BoundTargets.Add(Target);
    BoundFunctionNames.Add(FunctionName);
    return true;
}

/** 按记录逐个解绑按钮委托并清空绑定列表。 */
void ULyraUIControlBinding::UnbindAll()
{
    const int32 BindingCount = FMath::Min3(BoundButtons.Num(), BoundTargets.Num(), BoundFunctionNames.Num());
    for (int32 Index = 0; Index < BindingCount; ++Index)
    {
        UButton* Button = BoundButtons[Index].Get();
        UObject* Target = BoundTargets[Index].Get();
        if (Button && Target)
        {
            FScriptDelegate Delegate;
            Delegate.BindUFunction(Target, BoundFunctionNames[Index]);
            Button->OnClicked.Remove(Delegate);
        }
    }

    BoundButtons.Empty();
    BoundTargets.Empty();
    BoundFunctionNames.Empty();
}

/** UObject 销毁时解除所有控件绑定。 */
void ULyraUIControlBinding::BeginDestroy()
{
    UnbindAll();
    Super::BeginDestroy();
}
