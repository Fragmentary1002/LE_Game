#include "LyraUIViewController.h"

#include "LyraUIView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIViewController)

/** 保存所属界面并初始化业务数据。 */
void ULyraUIViewController::InitializeController(ULyraUIView* InView, UObject* Context)
{
    View = InView;
    OnInit(Context);
}

/** 返回仍然有效的所属界面。 */
ULyraUIView* ULyraUIViewController::GetView() const
{
    return View.Get();
}

/** 使用所属 View 的世界，类默认对象保持无世界。 */
UWorld* ULyraUIViewController::GetWorld() const
{
    return !HasAnyFlags(RF_ClassDefaultObject) && View.IsValid() ? View->GetWorld() : nullptr;
}
