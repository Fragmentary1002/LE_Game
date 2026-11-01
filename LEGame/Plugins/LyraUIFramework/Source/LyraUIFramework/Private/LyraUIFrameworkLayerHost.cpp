#include "LyraUIFrameworkLayerHost.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "LyraUIFrameworkAdapterLibrary.h"
#include "LyraUIFrameworkSettings.h"
#include "LyraUIView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkLayerHost)

/** 创建全屏宿主并裁切固定宽高适配产生的溢出。 */
void ULyraUIFrameworkLayerHost::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    CanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UIFrameworkLayerCanvas"));
    WidgetTree->RootWidget = CanvasPanel;
    SetClipping(EWidgetClipping::ClipToBounds);
}

/** 将界面和配置作为同一条记录保存。 */
void ULyraUIFrameworkLayerHost::AddView(ULyraUIView* View, const FLyraUIConfigRow& Config)
{
    if (!IsValid(View) || !CanvasPanel || Entries.ContainsByPredicate([View](const FLyraUIFrameworkLayerEntry& Entry) { return Entry.View == View; }))
    {
        return;
    }

    FLyraUIFrameworkLayerEntry Entry;
    Entry.View = View;
    Entry.Config = Config;
    Entry.Order = ++NextOrder;
    if (Config.BlackType != ELyraUIFrameworkBlackType::None)
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
void ULyraUIFrameworkLayerHost::RemoveView(ULyraUIView* View)
{
    const int32 Index = Entries.IndexOfByPredicate([View](const FLyraUIFrameworkLayerEntry& Entry) { return Entry.View == View; });
    if (Index == INDEX_NONE)
    {
        return;
    }
    const FLyraUIFrameworkLayerEntry Entry = Entries[Index];
    Entries.RemoveAt(Index);
    if (Entry.Background)
    {
        CanvasPanel->RemoveChild(Entry.Background);
    }
    CanvasPanel->RemoveChild(View);
    RefreshViews();
}

/** 置顶采用运行时排序值，不改变配置或拆卸控件树。 */
void ULyraUIFrameworkLayerHost::BringViewToFront(ULyraUIView* View)
{
    int32 HighestZOrder = MIN_int32;
    for (const FLyraUIFrameworkLayerEntry& Entry : Entries)
    {
        HighestZOrder = FMath::Max(HighestZOrder, Entry.Config.ZOrder);
    }
    for (FLyraUIFrameworkLayerEntry& Entry : Entries)
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
void ULyraUIFrameworkLayerHost::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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
void ULyraUIFrameworkLayerHost::RefreshViews()
{
    if (bRefreshing)
    {
        bRefreshRequested = true;
        return;
    }
    TGuardValue<bool> RefreshGuard(bRefreshing, true);
    bRefreshRequested = false;
    TArray<FLyraUIFrameworkLayerEntry> Snapshot = Entries;
    Snapshot.Sort([](const FLyraUIFrameworkLayerEntry& A, const FLyraUIFrameworkLayerEntry& B)
    {
        return A.Config.ZOrder == B.Config.ZOrder ? A.Order < B.Order : A.Config.ZOrder < B.Config.ZOrder;
    });
    int32 LastFullScreen = INDEX_NONE;
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        if (Snapshot[Index].Config.Presentation == ELyraUIFrameworkViewPresentation::FullScreen)
        {
            LastFullScreen = Index;
        }
    }
    const FVector2D DesignSize = GetDefault<ULyraUIFrameworkSettings>()->DesignResolution;
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        const FLyraUIFrameworkLayerEntry& Entry = Snapshot[Index];
        const bool bVisible = LastFullScreen == INDEX_NONE || Index >= LastFullScreen;
        if (Entry.Background)
        {
            Entry.Background->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Entry.Background->Slot))
            {
                Slot->SetAnchors(FAnchors(0, 0, 1, 1));
                Slot->SetOffsets(FMargin(0));
                Slot->SetZOrder(Index * 2);
            }
        }
        if (Entry.View)
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Entry.View->Slot))
            {
                Slot->SetAnchors(FAnchors(0, 0, 1, 1));
                Slot->SetOffsets(ULyraUIFrameworkAdapterLibrary::CalculateBlackBarPadding(CachedViewportSize, DesignSize, Entry.Config.BlackType));
                Slot->SetZOrder(Index * 2 + 1);
            }
        }
    }
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        ULyraUIView* View = Snapshot[Index].View;
        if (IsValid(View) && Entries.ContainsByPredicate([View](const FLyraUIFrameworkLayerEntry& Entry) { return Entry.View == View; }))
        {
            View->SetFrameworkVisible(LastFullScreen == INDEX_NONE || Index >= LastFullScreen);
            if (bRefreshRequested)
            {
                break;
            }
        }
    }
}
