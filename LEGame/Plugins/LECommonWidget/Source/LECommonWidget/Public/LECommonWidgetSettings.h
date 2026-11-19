#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LECommonWidgetSettings.generated.h"

class ULECommonWidgetFactory;

/** 通用控件的项目级配置，可独立于界面流程框架使用。 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "LECommonWidget"))
class LECOMMONWIDGET_API ULECommonWidgetSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 可覆盖基础 Widget、Image 和 TextBlock 创建规则的工厂类。 */
    UPROPERTY(Config, EditAnywhere, Category = "Creation")
    TSubclassOf<ULECommonWidgetFactory> WidgetFactoryClass;
};
