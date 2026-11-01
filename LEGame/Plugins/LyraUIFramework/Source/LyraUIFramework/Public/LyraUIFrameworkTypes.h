#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "LyraUIFrameworkTypes.generated.h"

class ULyraUIView;
class ULyraUIViewController;

/** UI 业务分类，用于表达界面在项目中的角色。 */
UENUM(BlueprintType)
enum class ELyraUIFrameworkType : uint8
{
    View UMETA(DisplayName = "View"),
    HUD UMETA(DisplayName = "HUD"),
    Popup UMETA(DisplayName = "Popup"),
    Modal UMETA(DisplayName = "Modal"),
    SubView UMETA(DisplayName = "Sub View")
};

/** UI 打开过程的运行状态。 */
UENUM(BlueprintType)
enum class ELyraUIFrameworkViewState : uint8
{
    Unloaded UMETA(DisplayName = "Unloaded"),
    Loading UMETA(DisplayName = "Loading"),
    Open UMETA(DisplayName = "Open"),
    Suspended UMETA(DisplayName = "Suspended"),
    Closing UMETA(DisplayName = "Closing"),
    Closed UMETA(DisplayName = "Closed"),
    Failed UMETA(DisplayName = "Failed")
};

/** UI 的覆盖策略；全屏界面会隐藏同层下方的界面，窗口界面保留下方界面。 */
UENUM(BlueprintType)
enum class ELyraUIFrameworkViewPresentation : uint8
{
    FullScreen UMETA(DisplayName = "Full Screen"),
    Window UMETA(DisplayName = "Window")
};

/** UI 黑边适配模式。 */
UENUM(BlueprintType)
enum class ELyraUIFrameworkBlackType : uint8
{
    None UMETA(DisplayName = "None"),
    Height UMETA(DisplayName = "Height"),
    Width UMETA(DisplayName = "Width"),
    Auto UMETA(DisplayName = "Auto")
};

/** 同一 UI 再次打开时的实例处理方式。 */
UENUM(BlueprintType)
enum class ELyraUIFrameworkDuplicatePolicy : uint8
{
    Reuse UMETA(DisplayName = "Reuse Existing"),
    BringToFront UMETA(DisplayName = "Bring Existing To Front"),
    Restart UMETA(DisplayName = "Close Existing And Reopen"),
    AllowMultiple UMETA(DisplayName = "Allow Multiple")
};

/**
 * UI 配置表行。项目只需配置这张表，运行时由 UI Manager 根据 UIName 打开界面。
 */
USTRUCT(BlueprintType)
struct LYRAUIFRAMEWORK_API FLyraUIConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    /** UI 的稳定标识，业务代码只依赖此字段。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    FName UIName;

    /** UI 的本地化显示名称。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    FText DisplayName;

    /** UI 的 CommonActivatableWidget 蓝图类。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AssetBundles = "Client"))
    TSoftClassPtr<ULyraUIView> WBPPath;

    /** 可选业务控制器，跨项目框架不依赖具体业务 View。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<ULyraUIViewController> ControllerClass;

    /** UI 业务类型。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    ELyraUIFrameworkType UIType = ELyraUIFrameworkType::View;

    /** UI 所属的 CommonUI Layer。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Layer"))
    FGameplayTag UILayer;

    /** 同一 Layer 内的逻辑排序值，越大越靠上。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    int32 ZOrder = 0;

    /** 覆盖策略；窗口界面不会隐藏下层界面。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    ELyraUIFrameworkViewPresentation Presentation = ELyraUIFrameworkViewPresentation::FullScreen;

    /** 同一界面再次打开时的处理策略。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Behavior")
    ELyraUIFrameworkDuplicatePolicy DuplicatePolicy = ELyraUIFrameworkDuplicatePolicy::Reuse;

    /** 是否为常驻 UI；关闭全部时默认保留。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Behavior")
    bool bResident = false;

    /** 是否在打开前挂起玩家输入。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Behavior")
    bool bSuspendInputUntilLoaded = true;

    /** 是否在打开后自动请求焦点。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Behavior")
    bool bRequestFocus = true;

    /** 是否使用 CommonUI 返回动作请求关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Behavior")
    bool bCloseOnBack = true;

    /** 是否将界面内容限制到设备安全区。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Adaptation")
    bool bUseSafeArea = true;

    /** 黑边适配策略。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Adaptation")
    ELyraUIFrameworkBlackType BlackType = ELyraUIFrameworkBlackType::None;
};
