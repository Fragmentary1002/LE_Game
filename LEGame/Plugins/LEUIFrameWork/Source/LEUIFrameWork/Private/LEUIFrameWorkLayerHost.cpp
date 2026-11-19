#include "LEUIFrameWorkLayerHost.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "LEUIFrameWorkAdapterLibrary.h"
#include "LEUIFrameWorkSettings.h"
#include "LEUIView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEUIFrameWorkLayerHost)

/** 创建全屏宿主并裁切固定宽高适配产生的溢出。 */
void ULEUIFrameWorkLayerHost::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    CanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UIFrameworkLayerCanvas"));
    WidgetTree->RootWidget = CanvasPanel;
    SetClipping(EWidgetClipping::ClipToBounds);
}

/** 将界面和配置作为同一条记录保存。 */
void ULEUIFrameWorkLayerHost::AddView(ULEUIView* View, const FLEUIConfigRow& Config)
{
    if (!IsValid(View) || !CanvasPanel || Entries.ContainsByPredicate([View](const FLEUIFrameWorkLayerEntry& Entry) { return Entry.View == View; }))
    {
        return;
    }

    FLEUIFrameWorkLayerEntry Entry;
    Entry.View = View;
    Entry.Config = Config;
    Entry.Order = ++NextOrder;
    if (Config.BlackType != ELEUIFrameWorkBlackType::None)
    {
        Entry.Background = WidgetTree->ConstructWidget<UBorder>();
        Entry.Background->SetBrushColor(FLinearColor::Black);
        Entry.Background->SetVisibility(ESlateVisibility::HitTestInvisible);
        CanvasPanel->AddChildToCanvas(Entry.Background);
    }
    Entries.Add(Entry);
    CanvasPanel->AddChildToCanvas(View);
    RefreshViews();
}

/** 先移除记录，再释放 Slate 子项，避免回调查询到过期记录。 */
void ULEUIFrameWorkLayerHost::RemoveView(ULEUIView* View)
{
    const int32 Index = Entries.IndexOfByPredicate([View](const FLEUIFrameWorkLayerEntry& Entry) { return Entry.View == View; });
    if (Index == INDEX_NONE)
    {
        return;
    }
    const FLEUIFrameWorkLayerEntry Entry = Entries[Index];
    Entries.RemoveAt(Index);
    if (Entry.Background)
    {
        CanvasPanel->RemoveChild(Entry.Background);
    }
    CanvasPanel->RemoveChild(View);
    RefreshViews();
}

/** 置顶采用运行时排序值，不改变配置或拆卸控件树。 */
void ULEUIFrameWorkLayerHost::BringViewToFront(ULEUIView* View)
{
    int32 HighestZOrder = MIN_int32;
    for (const FLEUIFrameWorkLayerEntry& Entry : Entries)
    {
        HighestZOrder = FMath::Max(HighestZOrder, Entry.Config.ZOrder);
    }
    for (FLEUIFrameWorkLayerEntry& Entry : Entries)
    {
        if (Entry.View == View)
        {
            Entry.Config.ZOrder = HighestZOrder;
            Entry.Order = ++NextOrder;
            break;
        }
    }
    RefreshViews();
}

/** 布局尺寸变化或重入刷新被延后时重算黑边。 */
void ULEUIFrameWorkLayerHost::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    const FVector2D Size = MyGeometry.GetLocalSize();
    if (bRefreshRequested || !Size.Equals(CachedViewportSize, 0.5f))
    {
        CachedViewportSize = Size;
        RefreshViews();
    }
}

/** 完成所有槽位写入后再回调业务，业务修改数组不会破坏本轮遍历。 */
void ULEUIFrameWorkLayerHost::RefreshViews()
{
    if (bRefreshing)
    {
        bRefreshRequested = true;
        return;
    }
    TGuardValue<bool> RefreshGuard(bRefreshing, true);
    bRefreshRequested = false;
    TArray<FLEUIFrameWorkLayerEntry> Snapshot = Entries;
    Snapshot.Sort([](const FLEUIFrameWorkLayerEntry& A, const FLEUIFrameWorkLayerEntry& B)
    {
        return A.Config.ZOrder == B.Config.ZOrder ? A.Order < B.Order : A.Config.ZOrder < B.Config.ZOrder;
    });
    int32 LastFullScreen = INDEX_NONE;
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        if (Snapshot[Index].Config.Presentation == ELEUIFrameWorkViewPresentation::FullScreen)
        {
            LastFullScreen = Index;
        }
    }
    const FVector2D DesignSize = GetDefault<ULEUIFrameWorkSettings>()->DesignResolution;
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        const FLEUIFrameWorkLayerEntry& Entry = Snapshot[Index];
        const bool bVisible = LastFullScreen == INDEX_NONE || Index >= LastFullScreen;
        if (Entry.Background)
        {
            Entry.Background->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Entry.Background->Slot))
            {
                CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
                CanvasSlot->SetOffsets(FMargin(0));
                CanvasSlot->SetZOrder(Index * 2);
            }
        }
        if (Entry.View)
        {
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Entry.View->Slot))
            {
                CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
                CanvasSlot->SetOffsets(ULEUIFrameWorkAdapterLibrary::CalculateBlackBarPadding(CachedViewportSize, DesignSize, Entry.Config.BlackType));
                CanvasSlot->SetZOrder(Index * 2 + 1);
            }
        }
    }
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        ULEUIView* View = Snapshot[Index].View;
        if (IsValid(View) && Entries.ContainsByPredicate([View](const FLEUIFrameWorkLayerEntry& Entry) { return Entry.View == View; }))
        {
            View->SetFrameworkVisible(LastFullScreen == INDEX_NONE || Index >= LastFullScreen);
            if (bRefreshRequested)
            {
                break;
            }
        }
    }
}
