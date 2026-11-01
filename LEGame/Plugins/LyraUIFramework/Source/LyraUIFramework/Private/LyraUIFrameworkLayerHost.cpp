#include "LyraUIFrameworkLayerHost.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "LyraUIFrameworkAdapterLibrary.h"
#include "LyraUIView.h"
#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkLayerHost)

/** 在宿主初始化时创建全屏 Canvas 根控件。 */
void ULyraUIFrameworkLayerHost::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    CanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UIFrameworkLayerCanvas"));
    WidgetTree->RootWidget = CanvasPanel;
    SetClipping(EWidgetClipping::ClipToBounds);
}

/** 将界面添加到 Canvas 并刷新层级和遮挡状态。 */
void ULyraUIFrameworkLayerHost::AddView(ULyraUIView* View, ELyraUIFrameworkViewPresentation Presentation, int32 ZOrder, ELyraUIFrameworkBlackType BlackType, FVector2D DesignResolution)
{
    if (!IsValid(View) || !CanvasPanel || Views.Contains(View))
    {
        return;
    }

    Views.Add(View);
    Presentations.Add(Presentation);
    ZOrders.Add(ZOrder);
    BlackTypes.Add(BlackType);
    DesignResolutions.Add(DesignResolution);

    UBorder* Background = nullptr;
    if (BlackType != ELyraUIFrameworkBlackType::None)
    {
        Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        CanvasPanel->AddChildToCanvas(Background);
        Background->SetBrushColor(FLinearColor::Black);
    }
    BlackBarBackgrounds.Add(Background);
    CanvasPanel->AddChildToCanvas(View);
    View->NotifyFrameworkOpened();
    RefreshViews();
}

/** 移除界面及其排序数据，并刷新剩余界面。 */
void ULyraUIFrameworkLayerHost::RemoveView(ULyraUIView* View)
{
    const int32 Index = Views.IndexOfByKey(View);
    if (Index == INDEX_NONE)
    {
        return;
    }

    if (CanvasPanel)
    {
        if (BlackBarBackgrounds[Index])
        {
            CanvasPanel->RemoveChild(BlackBarBackgrounds[Index]);
        }
        CanvasPanel->RemoveChild(View);
    }
    Views.RemoveAt(Index);
    Presentations.RemoveAt(Index);
    ZOrders.RemoveAt(Index);
    BlackBarBackgrounds.RemoveAt(Index);
    BlackTypes.RemoveAt(Index);
    DesignResolutions.RemoveAt(Index);
    RefreshViews();
}

/** 视口尺寸变化时重新计算自适应黑边。 */
void ULyraUIFrameworkLayerHost::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const FVector2D ViewportSize = MyGeometry.GetLocalSize();
    if (!ViewportSize.Equals(CachedViewportSize, 1.0f))
    {
        CachedViewportSize = ViewportSize;
        RefreshViews();
    }
}

/** 按配置排序 Canvas 子项并隐藏被更高全屏界面遮挡的内容。 */
void ULyraUIFrameworkLayerHost::RefreshViews()
{
    const int32 Count = Views.Num();
    if (Count == 0 || Presentations.Num() != Count || ZOrders.Num() != Count || BlackBarBackgrounds.Num() != Count ||
        BlackTypes.Num() != Count || DesignResolutions.Num() != Count)
    {
        return;
    }

    TArray<int32> SortedIndices;
    SortedIndices.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        SortedIndices.Add(Index);
    }
    SortedIndices.Sort([this](int32 Left, int32 Right)
    {
        return ZOrders[Left] == ZOrders[Right] ? Left < Right : ZOrders[Left] < ZOrders[Right];
    });

    int32 HighestFullScreenIndex = INDEX_NONE;
    for (int32 SortedIndex = 0; SortedIndex < SortedIndices.Num(); ++SortedIndex)
    {
        if (Presentations[SortedIndices[SortedIndex]] == ELyraUIFrameworkViewPresentation::FullScreen)
        {
            HighestFullScreenIndex = SortedIndex;
        }
    }

    for (int32 SortedIndex = 0; SortedIndex < SortedIndices.Num(); ++SortedIndex)
    {
        const int32 ViewIndex = SortedIndices[SortedIndex];
        if (ULyraUIView* View = Views[ViewIndex])
        {
            const int32 CanvasZOrder = SortedIndex * 2;
            if (UBorder* Background = BlackBarBackgrounds[ViewIndex])
            {
                if (UCanvasPanelSlot* BackgroundSlot = Cast<UCanvasPanelSlot>(Background->Slot))
                {
                    BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
                    BackgroundSlot->SetOffsets(FMargin(0.0f));
                    BackgroundSlot->SetZOrder(CanvasZOrder);
                }
            }
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(View->Slot))
            {
                CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
                const FMargin Padding = ULyraUIFrameworkAdapterLibrary::CalculateBlackBarPadding(CachedViewportSize, DesignResolutions[ViewIndex], BlackTypes[ViewIndex]);
                CanvasSlot->SetOffsets(FMargin(Padding.Left, Padding.Top, -Padding.Right, -Padding.Bottom));
                CanvasSlot->SetZOrder(CanvasZOrder + 1);
            }
            View->SetFrameworkVisible(HighestFullScreenIndex == INDEX_NONE || SortedIndex >= HighestFullScreenIndex);
        }
    }
}
