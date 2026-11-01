#include "LyraUIFrameworkRootLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "LyraUIFrameworkSettings.h"
#include "LyraUIFrameworkTags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkRootLayout)

/** 转交 CommonGame RootLayout 初始化参数。 */
ULyraUIFrameworkRootLayout::ULyraUIFrameworkRootLayout(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

/** 创建框架标准层和 Lyra 兼容层并向 RootLayout 注册。 */
void ULyraUIFrameworkRootLayout::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("原生 UI Framework RootLayout 不能覆盖已有设计器布局，请使用独立的 UPrimaryGameLayout 子类。"));
        return;
    }
    LayerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UIFrameworkRootLayers"));
    WidgetTree->RootWidget = LayerCanvas;
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    LayerCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    const ULyraUIFrameworkSettings* Settings = GetDefault<ULyraUIFrameworkSettings>();
    const TArray<TPair<FGameplayTag, FName>> LayerDefinitions =
    {
        { Settings->BackgroundLayer, TEXT("UIFrameworkBackgroundLayer") },
        { LyraUIFrameworkTags::Game, TEXT("UIFrameworkGameLayer") },
        { Settings->NormalLayer, TEXT("UIFrameworkNormalLayer") },
        { Settings->InfoLayer, TEXT("UIFrameworkInfoLayer") },
        { LyraUIFrameworkTags::GameMenu, TEXT("UIFrameworkGameMenuLayer") },
        { LyraUIFrameworkTags::Menu, TEXT("UIFrameworkMenuLayer") },
        { Settings->TopLayer, TEXT("UIFrameworkTopLayer") },
        { Settings->TipLayer, TEXT("UIFrameworkTipLayer") },
        { LyraUIFrameworkTags::Modal, TEXT("UIFrameworkModalLayer") }
    };

    TSet<FGameplayTag> RegisteredLayers;
    for (int32 Index = 0; Index < LayerDefinitions.Num(); ++Index)
    {
        const FGameplayTag& LayerTag = LayerDefinitions[Index].Key;
        if (!LayerTag.IsValid() || RegisteredLayers.Contains(LayerTag))
        {
            continue;
        }
        UCommonActivatableWidgetStack* LayerStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(
            UCommonActivatableWidgetStack::StaticClass(), LayerDefinitions[Index].Value);
        if (!LayerStack)
        {
            continue;
        }
        UCanvasPanelSlot* Slot = LayerCanvas->AddChildToCanvas(LayerStack);
        LayerStack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        Slot->SetOffsets(FMargin(0.0f));
        Slot->SetZOrder(Index);
        RegisterLayer(LayerTag, LayerStack);
        RegisteredLayers.Add(LayerTag);
        if (LayerTag == LyraUIFrameworkTags::Modal.GetTag() && Settings->ModalLayer.IsValid() &&
            !RegisteredLayers.Contains(Settings->ModalLayer))
        {
            RegisterLayer(Settings->ModalLayer, LayerStack);
            RegisteredLayers.Add(Settings->ModalLayer);
        }
    }
}
