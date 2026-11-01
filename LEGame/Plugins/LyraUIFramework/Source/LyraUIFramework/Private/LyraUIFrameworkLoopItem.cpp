#include "LyraUIFrameworkLoopItem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkLoopItem)

/** 保存该次循环展示对应的源对象和索引。 */
void ULyraUIFrameworkLoopItem::Initialize(UObject* InSourceItem, int32 InSourceIndex)
{
    SourceItem = InSourceItem;
    SourceIndex = InSourceIndex;
}
