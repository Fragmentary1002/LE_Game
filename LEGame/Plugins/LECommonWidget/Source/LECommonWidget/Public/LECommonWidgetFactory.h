#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LECommonWidgetFactory.generated.h"

class UImage;
class UTextBlock;
class UWidget;
class UWidgetTree;

/** 可按项目规范覆盖 UMG 常用基础控件创建方式的工厂。 */
UCLASS(BlueprintType, Blueprintable)
class LECOMMONWIDGET_API ULECommonWidgetFactory : public UObject
{
    GENERATED_BODY()

public:
    /** 使用指定 UWidget 子类创建控件，供项目统一替换控件类别和默认规则。 */
    UFUNCTION(BlueprintNativeEvent, Category = "LE UI|Creation")
    UWidget* CreateWidget(UWidgetTree* WidgetTree, TSubclassOf<UWidget> WidgetClass, FName WidgetName) const;
    /** 默认按指定控件类构建实例，供项目覆盖创建规则。 */
    virtual UWidget* CreateWidget_Implementation(UWidgetTree* WidgetTree, TSubclassOf<UWidget> WidgetClass, FName WidgetName) const;

    /** 根据 WidgetTree 创建 Image 控件。 */
    UFUNCTION(BlueprintNativeEvent, Category = "LE UI|Creation")
    UImage* CreateImage(UWidgetTree* WidgetTree, FName WidgetName) const;
    /** 默认通过通用创建入口构建 Image。 */
    virtual UImage* CreateImage_Implementation(UWidgetTree* WidgetTree, FName WidgetName) const;

    /** 根据 WidgetTree 创建 TextBlock 控件。 */
    UFUNCTION(BlueprintNativeEvent, Category = "LE UI|Creation")
    UTextBlock* CreateTextBlock(UWidgetTree* WidgetTree, FName WidgetName) const;
    /** 默认通过通用创建入口构建 TextBlock。 */
    virtual UTextBlock* CreateTextBlock_Implementation(UWidgetTree* WidgetTree, FName WidgetName) const;
};
