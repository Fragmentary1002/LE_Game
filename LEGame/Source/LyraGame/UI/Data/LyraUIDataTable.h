#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "LyraUIDataTable.generated.h"

class UCommonActivatableWidget;

/** UI 配置类型，用于区分界面在业务上的用途。 */
UENUM(BlueprintType)
enum class ELyraUIType : uint8
{
	View UMETA(DisplayName = "View"),
	HUD UMETA(DisplayName = "HUD"),
	Popup UMETA(DisplayName = "Popup"),
	Modal UMETA(DisplayName = "Modal")
};

/** Lyra 通用 UI 配置表行，用于驱动 OpenView 的界面查找和打开参数。 */
USTRUCT(BlueprintType)
struct LYRAGAME_API FLyraUIDataTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** UI 的稳定标识，建议使用不会随多语言变化的名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FName UIName;

	/** UI 的显示名称，供编辑器、日志或调试界面使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FText DisplayName;

	/** UI 对应的 CommonActivatableWidget 蓝图类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> WBPPath;

	/** UI 的业务类型，不直接决定打开逻辑。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	ELyraUIType UIType = ELyraUIType::View;

	/** UI 要压入的 CommonUI Layer，例如 UI.Layer.GameMenu。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag UILayer;

	/** UI 的显示层级；使用 Layer 栈时，实际顺序仍由 Layer 容器管理。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 ZOrder = 0;

	/** 是否允许同一玩家同时打开多个此 UI 实例，由 OpenView 实现该策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Behavior")
	bool bAllowMultipleInstances = false;

	/** 异步加载界面类时是否暂时挂起玩家输入，由 OpenView 传递给异步打开接口。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Behavior")
	bool bSuspendInputUntilLoaded = true;
};
