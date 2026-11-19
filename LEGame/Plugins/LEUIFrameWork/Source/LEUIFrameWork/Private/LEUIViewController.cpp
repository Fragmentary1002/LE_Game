#include "LEUIViewController.h"

#include "LEUIView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEUIViewController)

/** 保存所属界面并初始化业务数据。 */
void ULEUIViewController::InitializeController(ULEUIView* InView, UObject* Context)
{
    View = InView;
    OnInit(Context);
}

/** 返回仍然有效的所属界面。 */
ULEUIView* ULEUIViewController::GetView() const
{
    return View.Get();
}

/** 使用所属 View 的世界，类默认对象保持无世界。 */
UWorld* ULEUIViewController::GetWorld() const
{
    return !HasAnyFlags(RF_ClassDefaultObject) && View.IsValid() ? View->GetWorld() : nullptr;
}
