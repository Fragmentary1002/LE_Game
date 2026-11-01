#include "LyraUIFrameworkWidgetCreationLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "LyraUIFrameworkSettings.h"
#include "LyraUIFrameworkWidgetFactory.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkWidgetCreationLibrary)

/** 调用项目控件工厂创建指定 UWidget 子类。 */
UWidget* ULyraUIFrameworkWidgetCreationLibrary::CreateWidget(UWidgetTree* WidgetTree, TSubclassOf<UWidget> WidgetClass, FName WidgetName)
{
    if (!WidgetTree || !WidgetClass)
    {
        return nullptr;
    }
    const TSubclassOf<ULyraUIFrameworkWidgetFactory> FactoryClass = GetDefault<ULyraUIFrameworkSettings>()->WidgetFactoryClass;
    ULyraUIFrameworkWidgetFactory* Factory = NewObject<ULyraUIFrameworkWidgetFactory>(GetTransientPackage(), FactoryClass ? FactoryClass : ULyraUIFrameworkWidgetFactory::StaticClass());
    return Factory ? Factory->CreateWidget(WidgetTree, WidgetClass, WidgetName) : nullptr;
}

/** 调用项目控件工厂创建 Image。 */
UImage* ULyraUIFrameworkWidgetCreationLibrary::CreateImage(UWidgetTree* WidgetTree, FName WidgetName)
{
    if (!WidgetTree)
    {
        return nullptr;
    }
    const TSubclassOf<ULyraUIFrameworkWidgetFactory> FactoryClass = GetDefault<ULyraUIFrameworkSettings>()->WidgetFactoryClass;
    ULyraUIFrameworkWidgetFactory* Factory = NewObject<ULyraUIFrameworkWidgetFactory>(GetTransientPackage(), FactoryClass ? FactoryClass : ULyraUIFrameworkWidgetFactory::StaticClass());
    return Factory ? Factory->CreateImage(WidgetTree, WidgetName) : nullptr;
}

/** 调用项目控件工厂创建 TextBlock。 */
UTextBlock* ULyraUIFrameworkWidgetCreationLibrary::CreateTextBlock(UWidgetTree* WidgetTree, FName WidgetName)
{
    if (!WidgetTree)
    {
        return nullptr;
    }
    const TSubclassOf<ULyraUIFrameworkWidgetFactory> FactoryClass = GetDefault<ULyraUIFrameworkSettings>()->WidgetFactoryClass;
    ULyraUIFrameworkWidgetFactory* Factory = NewObject<ULyraUIFrameworkWidgetFactory>(GetTransientPackage(), FactoryClass ? FactoryClass : ULyraUIFrameworkWidgetFactory::StaticClass());
    return Factory ? Factory->CreateTextBlock(WidgetTree, WidgetName) : nullptr;
}
