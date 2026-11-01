#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "LyraUIFrameworkSettings.generated.h"

class UDataTable;
class ULyraUIFrameworkWidgetFactory;

/** Lyra UI Framework 的项目级配置。 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Lyra UI Framework"))
class LYRAUIFRAMEWORK_API ULyraUIFrameworkSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 提供插件自带的默认 Layer，无需项目重复配置。 */
    ULyraUIFrameworkSettings(const FObjectInitializer& ObjectInitializer);

    /** UI 配置 DataTable，行结构必须是 FLyraUIConfigRow。 */
    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSoftObjectPtr<UDataTable> UIConfigTable;

    /** 可覆盖 Image 和 TextBlock 创建规则的工厂类。 */
    UPROPERTY(Config, EditAnywhere, Category = "UI|Creation")
    TSubclassOf<ULyraUIFrameworkWidgetFactory> WidgetFactoryClass;

    /** World Space 场景 UI 使用的 Layer。 */
    UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
    FGameplayTag SceneLayer;

    /** 背景和黑边使用的 Layer。 */
    UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
    FGameplayTag BackgroundLayer;

    /** 普通全屏界面使用的 Layer。 */
    UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
    FGameplayTag NormalLayer;

    /** 普通界面上方的信息 Layer。 */
    UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
    FGameplayTag InfoLayer;

    /** Loading 等顶层界面使用的 Layer。 */
    UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
    FGameplayTag TopLayer;

    /** Toast 和提示使用的 Layer。 */
    UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
    FGameplayTag TipLayer;

    /** 模态确认框使用的 Layer。 */
    UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (Categories = "UI.Layer"))
    FGameplayTag ModalLayer;

    /** 黑边适配和 Canvas 视图区域使用的设计分辨率。 */
    UPROPERTY(Config, EditAnywhere, Category = "Adaptation", meta = (ClampMin = "1.0"))
    FVector2D DesignResolution = FVector2D(1920.0f, 1080.0f);

};
