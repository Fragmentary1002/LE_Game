#pragma once

#include "CoreMinimal.h"

class IAssetTools;
class UDataTable;
class UUserDefinedEnum;
class UWidgetBlueprint;

/** 隔离编辑器资产生成细节，不让运行时依赖编辑器模块。 */
namespace LyraUIFrameworkEditorTools
{
    /** 直接使用框架 View，或从普通 Widget 模板生成带框架父类的包装蓝图。 */
    UWidgetBlueprint* ResolveOrCreateView(UWidgetBlueprint* Template, IAssetTools& AssetTools);

    /** 在配置表同目录创建或读取专用 UIName 枚举，拒绝占用同名无关资产。 */
    UUserDefinedEnum* ResolveOrCreateNameEnum(UDataTable* DataTable, IAssetTools& AssetTools);

    /** 只追加缺少的标识枚举项，保留已有数值和顺序。 */
    bool EnsureNameEnumEntry(UUserDefinedEnum* NameEnum, FName UIName);
}
