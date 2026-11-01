#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LyraUIFrameworkWidgetCreationLibrary.generated.h"

class UImage;
class UTextBlock;
class UWidget;
class UWidgetTree;

/** 使用项目设置中的可覆盖工厂创建标准 UMG 控件。 */
UCLASS()
class LYRAUIFRAMEWORK_API ULyraUIFrameworkWidgetCreationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 调用项目控件工厂创建指定 UWidget 子类。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Creation")
    static UWidget* CreateWidget(UWidgetTree* WidgetTree, TSubclassOf<UWidget> WidgetClass, FName WidgetName);

    /** 调用项目控件工厂创建 Image。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Creation")
    static UImage* CreateImage(UWidgetTree* WidgetTree, FName WidgetName);

    /** 调用项目控件工厂创建 TextBlock。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Creation")
    static UTextBlock* CreateTextBlock(UWidgetTree* WidgetTree, FName WidgetName);
};
