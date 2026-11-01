#include "LyraUIFrameworkLayerHost.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "LyraUIView.h"
#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkLayerHost)

/** 在宿主初始化时创建全屏 Canvas 根控件。 */
void ULyraUIFrameworkLayerHost::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    CanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UIFrameworkLayerCanvas"));
    WidgetTree->RootWidget = CanvasPanel;
}

/** 将界面添加到 Canvas 并刷新层级和遮挡状态。 */
void ULyraUIFrameworkLayerHost::AddView(ULyraUIView* View, ELyraUIFrameworkViewPresentation Presentation, int32 ZOrder)
{
    if (!IsValid(View) || !CanvasPanel || Views.Contains(View))
    {
        return;
    }

    Views.Add(View);
    Presentations.Add(Presentation);
    ZOrders.Add(ZOrder);
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
        CanvasPanel->RemoveChild(View);
    }
    Views.RemoveAt(Index);
    Presentations.RemoveAt(Index);
    ZOrders.RemoveAt(Index);
    RefreshViews();
}

/** 按配置排序 Canvas 子项并隐藏被更高全屏界面遮挡的内容。 */
void ULyraUIFrameworkLayerHost::RefreshViews()
{
    const int32 Count = Views.Num();
    if (Count == 0 || Presentations.Num() != Count || ZOrders.Num() != Count)
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
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(View->Slot))
            {
                CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
                CanvasSlot->SetOffsets(FMargin(0.0f));
                CanvasSlot->SetZOrder(SortedIndex);
            }
            View->SetFrameworkVisible(HighestFullScreenIndex == INDEX_NONE || SortedIndex >= HighestFullScreenIndex);
        }
    }
}
