#pragma once

#include "CoreMinimal.h"
#include "CommonListView.h"
#include "LyraUIFrameworkLoopItem.h"
#include "LyraUIFrameworkLoopListView.generated.h"

/** 使用虚拟化 Entry 和周期性数据代理实现纵向循环列表。 */
UCLASS(BlueprintType)
class LYRAUIFRAMEWORK_API ULyraUIFrameworkLoopListView : public UCommonListView
{
    GENERATED_BODY()

public:
    /** 使用 CommonUI 列表初始化虚拟化循环列表。 */
    ULyraUIFrameworkLoopListView(const FObjectInitializer& ObjectInitializer);

    /** 设置源数据并生成多个循环周期的代理项。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Loop List")
    void SetSourceItems(const TArray<UObject*>& InSourceItems);

    /** 获取当前源数据数量。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Loop List")
    int32 GetSourceItemCount() const { return SourceItems.Num(); }

    /** 将循环代理或源索引映射回源对象。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Loop List")
    UObject* ResolveSourceItem(UObject* ListItem) const;

    /** 获取循环代理对应的源数组索引。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Loop List")
    int32 ResolveSourceIndex(UObject* ListItem) const;

protected:
    /** 在列表滚动接近副本边缘时平移一个完整数据周期。 */
    virtual void OnListViewScrolledInternal(float ItemOffset, float DistanceRemaining) override;

private:
    /** 源业务对象，修改列表后仍由此数组持有。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> SourceItems;

    /** 过滤无效对象后仍保留每个源对象的输入数组索引。 */
    UPROPERTY(Transient)
    TArray<int32> SourceIndices;

    /** 多个循环周期对应的独立虚拟化代理。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULyraUIFrameworkLoopItem>> LoopItems;

    /** 用于滚动的周期副本数，运行时会修正为 3 到 15 的奇数。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loop List", meta = (ClampMin = "3", ClampMax = "15", AllowPrivateAccess = "true"))
    int32 CycleCount = 5;

    /** 正在调整偏移时忽略递归滚动通知。 */
    bool bRecentering = false;
};
