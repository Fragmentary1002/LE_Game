#include "LyraUIFrameworkWidgetFactory.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkWidgetFactory)

/** 根据控件类和名称创建项目通用控件。 */
UWidget* ULyraUIFrameworkWidgetFactory::CreateWidget_Implementation(UWidgetTree* WidgetTree, TSubclassOf<UWidget> WidgetClass, FName WidgetName) const
{
    return WidgetTree && WidgetClass ? WidgetTree->ConstructWidget<UWidget>(WidgetClass, WidgetName) : nullptr;
}

/** 根据 WidgetTree 创建标准 Image 控件。 */
UImage* ULyraUIFrameworkWidgetFactory::CreateImage_Implementation(UWidgetTree* WidgetTree, FName WidgetName) const
{
    return Cast<UImage>(CreateWidget(WidgetTree, UImage::StaticClass(), WidgetName));
}

/** 根据 WidgetTree 创建标准 TextBlock 控件。 */
UTextBlock* ULyraUIFrameworkWidgetFactory::CreateTextBlock_Implementation(UWidgetTree* WidgetTree, FName WidgetName) const
{
    return Cast<UTextBlock>(CreateWidget(WidgetTree, UTextBlock::StaticClass(), WidgetName));
}
