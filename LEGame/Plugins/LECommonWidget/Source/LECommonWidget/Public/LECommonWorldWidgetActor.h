#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LECommonWorldWidgetActor.generated.h"

class ULocalPlayer;
class UUserWidget;
class UWidgetComponent;

/** 按玩家绑定并管理 World Space WidgetComponent 的场景 UI Actor。 */
UCLASS(BlueprintType)
class LECOMMONWIDGET_API ALECommonWorldWidgetActor : public AActor
{
    GENERATED_BODY()

public:
    /** 创建并配置 World Space WidgetComponent。 */
    ALECommonWorldWidgetActor(const FObjectInitializer& ObjectInitializer);

    /** 创建世界空间界面并绑定所属本地玩家。 */
    UFUNCTION(BlueprintCallable, Category = "LE UI|World Widget")
    void InitializeWorldWidget(TSubclassOf<UUserWidget> WidgetClass, ULocalPlayer* LocalPlayer);

    /** 获取当前场景界面控件实例。 */
    UFUNCTION(BlueprintPure, Category = "LE UI|World Widget")
    UUserWidget* GetWorldWidget() const;

protected:
    /** 移除控件实例，避免 Actor 销毁后残留 Slate 资源。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 该 Actor 独占的世界空间控件组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Widget", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetComponent> WidgetComponent;
};
