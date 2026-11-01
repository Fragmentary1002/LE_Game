#include "LyraUIFrameworkWorldWidgetActor.h"

#include "Components/WidgetComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkWorldWidgetActor)

/** 创建并配置 World Space WidgetComponent。 */
ALyraUIFrameworkWorldWidgetActor::ALyraUIFrameworkWorldWidgetActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WorldWidget"));
    SetRootComponent(WidgetComponent);
    WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
    WidgetComponent->SetWindowFocusable(false);
}

/** 创建世界空间界面并绑定所属本地玩家。 */
void ALyraUIFrameworkWorldWidgetActor::InitializeWorldWidget(TSubclassOf<UUserWidget> WidgetClass, ULocalPlayer* LocalPlayer)
{
    if (!WidgetComponent || !WidgetClass || !IsValid(LocalPlayer))
    {
        return;
    }
    APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld());
    if (!IsValid(PlayerController))
    {
        return;
    }
    WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
    WidgetComponent->SetWidget(nullptr);
    WidgetComponent->SetOwnerPlayer(LocalPlayer);
    WidgetComponent->SetWindowFocusable(false);
    UUserWidget* Widget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
    if (!Widget)
    {
        return;
    }
    WidgetComponent->SetWidget(Widget);
}

/** 获取当前场景界面控件实例。 */
UUserWidget* ALyraUIFrameworkWorldWidgetActor::GetWorldWidget() const
{
    return WidgetComponent ? WidgetComponent->GetUserWidgetObject() : nullptr;
}

/** 移除控件实例，避免 Actor 销毁后残留 Slate 资源。 */
void ALyraUIFrameworkWorldWidgetActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (WidgetComponent)
    {
        WidgetComponent->SetWidget(nullptr);
    }
    Super::EndPlay(EndPlayReason);
}
