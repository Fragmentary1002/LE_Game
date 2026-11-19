#include "LEUIFrameWorkRootLayout.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "LEUIFrameWorkSettings.h"
#include "LEUIFrameWorkTags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEUIFrameWorkRootLayout)

/** 转交 CommonGame RootLayout 初始化参数。 */
ULEUIFrameWorkRootLayout::ULEUIFrameWorkRootLayout(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

/** 创建框架标准层和 Lyra 兼容层并向 RootLayout 注册。 */
void ULEUIFrameWorkRootLayout::NativeOnInitialized()
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

    const ULEUIFrameWorkSettings* Settings = GetDefault<ULEUIFrameWorkSettings>();
    const TArray<TPair<FGameplayTag, FName>> LayerDefinitions =
    {
        { Settings->BackgroundLayer, TEXT("UIFrameworkBackgroundLayer") },
        { LEUIFrameWorkTags::Game, TEXT("UIFrameworkGameLayer") },
        { Settings->NormalLayer, TEXT("UIFrameworkNormalLayer") },
        { Settings->InfoLayer, TEXT("UIFrameworkInfoLayer") },
        { LEUIFrameWorkTags::GameMenu, TEXT("UIFrameworkGameMenuLayer") },
        { LEUIFrameWorkTags::Menu, TEXT("UIFrameworkMenuLayer") },
        { Settings->TopLayer, TEXT("UIFrameworkTopLayer") },
        { Settings->TipLayer, TEXT("UIFrameworkTipLayer") },
        { LEUIFrameWorkTags::Modal, TEXT("UIFrameworkModalLayer") }
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
        UCanvasPanelSlot* CanvasSlot = LayerCanvas->AddChildToCanvas(LayerStack);
        LayerStack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        CanvasSlot->SetOffsets(FMargin(0.0f));
        CanvasSlot->SetZOrder(Index);
        RegisterLayer(LayerTag, LayerStack);
        RegisteredLayers.Add(LayerTag);
        if (LayerTag == LEUIFrameWorkTags::Modal.GetTag() && Settings->ModalLayer.IsValid() &&
            !RegisteredLayers.Contains(Settings->ModalLayer))
        {
            RegisterLayer(Settings->ModalLayer, LayerStack);
            RegisteredLayers.Add(Settings->ModalLayer);
        }
    }
}
