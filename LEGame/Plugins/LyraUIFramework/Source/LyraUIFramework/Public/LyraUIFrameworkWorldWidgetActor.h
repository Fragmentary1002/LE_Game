#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LyraUIFrameworkWorldWidgetActor.generated.h"

class ULocalPlayer;
class UUserWidget;
class UWidgetComponent;

/** 按玩家绑定并管理 World Space WidgetComponent 的场景 UI Actor。 */
UCLASS(BlueprintType)
class LYRAUIFRAMEWORK_API ALyraUIFrameworkWorldWidgetActor : public AActor
{
    GENERATED_BODY()

public:
    /** 创建并配置 World Space WidgetComponent。 */
    ALyraUIFrameworkWorldWidgetActor(const FObjectInitializer& ObjectInitializer);

    /** 创建世界空间界面并绑定所属本地玩家。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|World Widget")
    void InitializeWorldWidget(TSubclassOf<UUserWidget> WidgetClass, ULocalPlayer* LocalPlayer);

    /** 获取当前场景界面控件实例。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|World Widget")
    UUserWidget* GetWorldWidget() const;

protected:
    /** 移除控件实例，避免 Actor 销毁后残留 Slate 资源。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 该 Actor 独占的世界空间控件组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Widget", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetComponent> WidgetComponent;
};
