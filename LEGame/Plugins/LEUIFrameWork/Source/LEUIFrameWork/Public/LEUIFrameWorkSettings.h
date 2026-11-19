#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "LEUIFrameWorkSettings.generated.h"

class UDataTable;

/** LEUIFrameWork 的项目级配置。 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "LEUIFrameWork"))
class LEUIFRAMEWORK_API ULEUIFrameWorkSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 提供插件自带的默认 Layer，无需项目重复配置。 */
    ULEUIFrameWorkSettings(const FObjectInitializer& ObjectInitializer);

    /** UI 配置 DataTable，行结构必须是 FLEUIConfigRow。 */
    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSoftObjectPtr<UDataTable> UIConfigTable;

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
