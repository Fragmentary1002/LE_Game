#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "LyraUIFrameworkSettings.generated.h"

class UDataTable;

/** Lyra UI Framework 的项目级配置。 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Lyra UI Framework"))
class LYRAUIFRAMEWORK_API ULyraUIFrameworkSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** UI 配置 DataTable，行结构必须是 FLyraUIConfigRow。 */
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

};
