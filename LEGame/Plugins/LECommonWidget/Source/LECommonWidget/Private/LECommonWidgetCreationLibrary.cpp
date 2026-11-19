#include "LECommonWidgetCreationLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "LECommonWidgetSettings.h"
#include "LECommonWidgetFactory.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LECommonWidgetCreationLibrary)

/** 调用项目控件工厂创建指定 UWidget 子类。 */
UWidget* ULECommonWidgetCreationLibrary::CreateWidget(UWidgetTree* WidgetTree, TSubclassOf<UWidget> WidgetClass, FName WidgetName)
{
    if (!WidgetTree || !WidgetClass)
    {
        return nullptr;
    }
    const TSubclassOf<ULECommonWidgetFactory> FactoryClass = GetDefault<ULECommonWidgetSettings>()->WidgetFactoryClass;
    ULECommonWidgetFactory* Factory = NewObject<ULECommonWidgetFactory>(GetTransientPackage(), FactoryClass ? FactoryClass.Get() : ULECommonWidgetFactory::StaticClass());
    return Factory ? Factory->CreateWidget(WidgetTree, WidgetClass, WidgetName) : nullptr;
}

/** 调用项目控件工厂创建 Image。 */
UImage* ULECommonWidgetCreationLibrary::CreateImage(UWidgetTree* WidgetTree, FName WidgetName)
{
    if (!WidgetTree)
    {
        return nullptr;
    }
    const TSubclassOf<ULECommonWidgetFactory> FactoryClass = GetDefault<ULECommonWidgetSettings>()->WidgetFactoryClass;
    ULECommonWidgetFactory* Factory = NewObject<ULECommonWidgetFactory>(GetTransientPackage(), FactoryClass ? FactoryClass.Get() : ULECommonWidgetFactory::StaticClass());
    return Factory ? Factory->CreateImage(WidgetTree, WidgetName) : nullptr;
}

/** 调用项目控件工厂创建 TextBlock。 */
UTextBlock* ULECommonWidgetCreationLibrary::CreateTextBlock(UWidgetTree* WidgetTree, FName WidgetName)
{
    if (!WidgetTree)
    {
        return nullptr;
    }
    const TSubclassOf<ULECommonWidgetFactory> FactoryClass = GetDefault<ULECommonWidgetSettings>()->WidgetFactoryClass;
    ULECommonWidgetFactory* Factory = NewObject<ULECommonWidgetFactory>(GetTransientPackage(), FactoryClass ? FactoryClass.Get() : ULECommonWidgetFactory::StaticClass());
    return Factory ? Factory->CreateTextBlock(WidgetTree, WidgetName) : nullptr;
}
