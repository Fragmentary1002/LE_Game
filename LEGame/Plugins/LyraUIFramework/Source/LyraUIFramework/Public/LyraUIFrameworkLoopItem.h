#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LyraUIFrameworkLoopItem.generated.h"

/** 循环列表中一次循环展示的代理对象。 */
UCLASS(BlueprintType)
class LYRAUIFRAMEWORK_API ULyraUIFrameworkLoopItem : public UObject
{
    GENERATED_BODY()

public:
    /** 获取代理对应的源数据对象。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Loop List")
    UObject* GetSourceItem() const { return SourceItem; }

    /** 获取源数据在传入数组中的索引。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Loop List")
    int32 GetSourceIndex() const { return SourceIndex; }

private:
    /** 由循环列表创建并赋值代理内容。 */
    void Initialize(UObject* InSourceItem, int32 InSourceIndex);

    UPROPERTY(Transient)
    TObjectPtr<UObject> SourceItem;

    UPROPERTY(Transient)
    int32 SourceIndex = INDEX_NONE;

    friend class ULyraUIFrameworkLoopListView;
};
