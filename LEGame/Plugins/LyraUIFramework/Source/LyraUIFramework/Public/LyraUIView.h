#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIView.generated.h"

class UWidgetAnimation;
class ULyraUIControlBinding;
class ULyraUISubView;
class ULyraUIViewController;
class ULyraUIViewHandle;

/** 实际业务界面；业务生命周期独立于 Slate 的重建和销毁。 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LYRAUIFRAMEWORK_API ULyraUIView : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 默认关闭自动激活，交由框架在完成初始化后管理。 */
    ULyraUIView(const FObjectInitializer& ObjectInitializer);

    /** 在挂接控件树之前注入名称、参数和配置。 */
    void InitializeView(FName InUIName, UObject* InOpenContext, const FLyraUIConfigRow& Config, ULyraUIViewHandle* InHandle);

    /** 获取配置中的稳定界面标识。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI")
    FName GetUIName() const { return UIName; }

    /** 获取当前打开参数，框架持有参数至界面释放。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI")
    UObject* GetOpenContext() const { return OpenContext; }

    /** 控制遮挡可见性和输入激活，不释放业务资源。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void SetFrameworkVisible(bool bVisible);

    /** 查询界面是否未被同层全屏界面覆盖。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI")
    bool IsFrameworkVisible() const { return bFrameworkVisible; }

    /** 请求通过 Manager 播放关闭动画并移除实例。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void RequestClose();

    /** 完成一次业务初始化和打开。 */
    void NotifyFrameworkOpened();

    /** 停止监听并通知业务开始关闭，只调用一次。 */
    void NotifyFrameworkClosing();

    /** 释放控件绑定、子界面和业务资源，只调用一次。 */
    void ReleaseFrameworkView();

    /** 播放可选打开动画。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Animation")
    bool PlayOpenAnimation();

    /** 播放可选关闭动画，结束后交给 Manager 最终移除。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Animation")
    bool PlayCloseAnimation();

    /** 返回由 View 持有的绑定助手。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Binding")
    ULyraUIControlBinding* GetControlBinding() const { return ControlBinding; }

    /** 执行一次初始化业务。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnInit();

    /** 打开时刷新业务数据。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnOpen();

    /** 被全屏界面覆盖时暂停业务。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnPause();

    /** 从覆盖恢复时继续业务。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnResume();

    /** 打开时绑定业务监听。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnAddListener();

    /** 关闭前解除业务监听。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnRemoveListener();

    /** 开始关闭时回调。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnClose();

    /** 实例逻辑释放时回调。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Lifecycle")
    void OnRelease();

protected:
    /** 默认返回动作关闭句柄，允许蓝图消费自定义返回行为。 */
    virtual bool NativeOnHandleBackAction() override;

    /** 清理外部停用产生的输入状态，再同步框架关闭。 */
    virtual void NativeOnDeactivated() override;

    /** Slate 重建不重复发送业务释放。 */
    virtual void NativeDestruct() override;

private:
    /** 动画结束后最终关闭，不再次播放动画。 */
    UFUNCTION()
    void HandleCloseAnimationFinished();

    /** 将蓝图内容根节点包装进设备安全区。 */
    void ApplySafeArea();

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Lyra UI", meta = (AllowPrivateAccess = "true"))
    FName UIName;

    UPROPERTY(Transient)
    TObjectPtr<UObject> OpenContext;

    UPROPERTY(Transient)
    TObjectPtr<ULyraUIControlBinding> ControlBinding;

    UPROPERTY(Transient)
    TObjectPtr<ULyraUIViewController> Controller;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ULyraUISubView>> SubViews;

    UPROPERTY(Transient)
    TWeakObjectPtr<ULyraUIViewHandle> ViewHandle;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Lyra UI|Animation", meta = (BindWidgetAnimOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetAnimation> OpenAnimation;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Lyra UI|Animation", meta = (BindWidgetAnimOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetAnimation> CloseAnimation;

    bool bOpened = false;
    bool bClosing = false;
    bool bReleased = false;
    bool bFrameworkVisible = true;
    bool bUpdatingActivation = false;
    bool bClosingWithAnimation = false;
    bool bRequestFocus = true;
};
