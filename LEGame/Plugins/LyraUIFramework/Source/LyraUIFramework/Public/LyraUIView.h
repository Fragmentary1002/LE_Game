#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "LyraUIFrameworkTypes.h"
#include "Animation/WidgetAnimation.h"
#include "LyraUIView.generated.h"

class UWidget;

/** 通用 UI 界面基类，统一界面生命周期和可见性控制。 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LYRAUIFRAMEWORK_API ULyraUIView : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 初始化界面实例的业务标识和打开参数。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void InitializeView(FName InUIName, UObject* InOpenContext, bool bInUseSafeArea, ELyraUIFrameworkBlackType InBlackType);

    /** 获取该界面的配置标识。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI")
    FName GetUIName() const { return UIName; }

    /** 获取打开该界面时传入的业务参数。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI")
    UObject* GetOpenContext() const { return OpenContext.Get(); }

    /** 控制框架是否渲染该界面，用于被全屏界面覆盖时降低开销。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void SetFrameworkVisible(bool bVisible);

    /** 获取框架当前是否允许该界面渲染。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI")
    bool IsFrameworkVisible() const { return bFrameworkVisible; }

    /** 请求关闭当前界面；实际移除由 UI Manager 执行。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void RequestClose();

    /** 由内部 Layer 宿主在首次插入时触发打开生命周期。 */
    void NotifyFrameworkOpened();

    /** 获取当前界面配置的黑边策略。 */
    ELyraUIFrameworkBlackType GetBlackType() const { return BlackType; }

    /** 播放配置的界面打开动画。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Animation")
    bool PlayOpenAnimation();

    /** 播放关闭动画，并在结束后关闭当前实例。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Animation")
    bool PlayCloseAnimation();

public:
    /** 界面初始化时调用一次。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnInit();

    /** 界面首次激活时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnOpen();

    /** 界面被上层界面覆盖时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnPause();

    /** 界面从覆盖状态恢复时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnResume();

    /** 界面关闭时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnClose();

    /** 界面销毁前调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnRelease();

    /** 关闭动画播放完成后请求 Manager 移除界面。 */
    UFUNCTION()
    void HandleCloseAnimationFinished();

protected:
    /** Widget 构造时触发初始化生命周期。 */
    virtual void NativeConstruct() override;

    /** 按配置将蓝图根控件置于安全区内。 */
    void ApplySafeArea();

    /** Widget 销毁时触发释放生命周期。 */
    virtual void NativeDestruct() override;

    /** Widget 激活时触发打开或恢复生命周期。 */
    virtual void NativeOnActivated() override;

    /** Widget 停用时触发暂停生命周期。 */
    virtual void NativeOnDeactivated() override;

private:
    /** UI 的配置标识。 */
    UPROPERTY(BlueprintReadOnly, Category = "Lyra UI", meta = (AllowPrivateAccess = "true"))
    FName UIName;

    /** 打开界面时传入的业务对象。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> OpenContext;

    /** 是否已经执行过 OnInit。 */
    bool bInitialized = false;

    /** 是否已经执行过 OnOpen。 */
    bool bOpened = false;

    /** 是否至少经历过一次 CommonUI 激活。 */
    bool bHasBeenActivated = false;

    /** 是否允许框架渲染。 */
    bool bFrameworkVisible = true;

    /** 初始化时是否将蓝图根控件包装进 SafeZone。 */
    bool bUseSafeArea = true;

    /** 当前界面使用的黑边适配策略。 */
    ELyraUIFrameworkBlackType BlackType = ELyraUIFrameworkBlackType::None;

    /** Widget Blueprint 可选绑定的打开动画。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Lyra UI|Animation", meta = (BindWidgetAnim))
    TObjectPtr<UWidgetAnimation> OpenAnimation;

    /** Widget Blueprint 可选绑定的关闭动画。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Lyra UI|Animation", meta = (BindWidgetAnim))
    TObjectPtr<UWidgetAnimation> CloseAnimation;

    /** 防止同一个界面重复登记关闭动画完成回调。 */
    bool bClosingWithAnimation = false;

    friend class ULyraUIFrameworkLayerHost;
};
