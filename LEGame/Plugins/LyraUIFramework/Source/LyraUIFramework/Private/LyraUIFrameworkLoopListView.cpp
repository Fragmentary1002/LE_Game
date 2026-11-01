#include "LyraUIFrameworkLoopListView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkLoopListView)

/** 将对象初始化参数转交给 CommonUI 列表基类。 */
ULyraUIFrameworkLoopListView::ULyraUIFrameworkLoopListView(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

/** 设置数据并生成多个周期的独立代理对象供虚拟化列表复用。 */
void ULyraUIFrameworkLoopListView::SetSourceItems(const TArray<UObject*>& InSourceItems)
{
    SourceItems.Reset(InSourceItems.Num());
    SourceIndices.Reset(InSourceItems.Num());
    for (int32 InputIndex = 0; InputIndex < InSourceItems.Num(); ++InputIndex)
    {
        UObject* Item = InSourceItems[InputIndex];
        if (IsValid(Item))
        {
            SourceItems.Add(Item);
            SourceIndices.Add(InputIndex);
        }
    }

    LoopItems.Reset();
    TArray<UObject*> ListItems;
    if (!SourceItems.IsEmpty())
    {
        const int32 SafeCycleCount = FMath::Clamp(CycleCount | 1, 3, 15);
        LoopItems.Reserve(SourceItems.Num() * SafeCycleCount);
        ListItems.Reserve(SourceItems.Num() * SafeCycleCount);
        for (int32 CycleIndex = 0; CycleIndex < SafeCycleCount; ++CycleIndex)
        {
            for (int32 SourceIndex = 0; SourceIndex < SourceItems.Num(); ++SourceIndex)
            {
                ULyraUIFrameworkLoopItem* LoopItem = NewObject<ULyraUIFrameworkLoopItem>(this);
                LoopItem->Initialize(SourceItems[SourceIndex], SourceIndices[SourceIndex]);
                LoopItems.Add(LoopItem);
                ListItems.Add(LoopItem);
            }
        }
    }
    SetListItems(ListItems);
}

/** 将列表中的循环代理映射回原始业务对象。 */
UObject* ULyraUIFrameworkLoopListView::ResolveSourceItem(UObject* ListItem) const
{
    if (const ULyraUIFrameworkLoopItem* LoopItem = Cast<ULyraUIFrameworkLoopItem>(ListItem))
    {
        return LoopItem->GetSourceItem();
    }
    return nullptr;
}

/** 将循环代理映射回原始输入数组的索引。 */
int32 ULyraUIFrameworkLoopListView::ResolveSourceIndex(UObject* ListItem) const
{
    if (const ULyraUIFrameworkLoopItem* LoopItem = Cast<ULyraUIFrameworkLoopItem>(ListItem))
    {
        return LoopItem->GetSourceIndex();
    }
    return INDEX_NONE;
}

/** 在首尾周期内移回列表中段并保留当前小数滚动偏移。 */
void ULyraUIFrameworkLoopListView::OnListViewScrolledInternal(float ItemOffset, float DistanceRemaining)
{
    Super::OnListViewScrolledInternal(ItemOffset, DistanceRemaining);
    if (bRecentering || SourceItems.IsEmpty() || GetOrientation() != Orient_Vertical)
    {
        return;
    }

    const float Period = static_cast<float>(SourceItems.Num());
    const float CurrentOffset = GetScrollOffset();
    const float MiddleOffset = Period * static_cast<float>(FMath::Clamp(CycleCount | 1, 3, 15) / 2);
    if (CurrentOffset < Period || CurrentOffset > Period * static_cast<float>(FMath::Max(1, (GetNumItems() / SourceItems.Num()) - 1)))
    {
        TGuardValue<bool> RecenterGuard(bRecentering, true);
        SetScrollOffset(MiddleOffset + FMath::Fmod(CurrentOffset, Period));
    }
}
