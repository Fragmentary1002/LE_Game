#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LyraUISubView.generated.h"

class ULyraUIView;

/** 由父界面直接拥有和管理、不进入全局 UI Manager 的子界面。 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LYRAUIFRAMEWORK_API ULyraUISubView : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 由父界面登记并执行一次初始化。 */
    void InitializeSubView(ULyraUIView* InOwningView);

    /** 显示子界面并触发打开回调。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|SubView")
    void OpenSubView();

    /** 隐藏子界面并触发关闭回调。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|SubView")
    void CloseSubView();

    /** 父界面释放时显式清理业务，不受 Slate 重建次数影响。 */
    void ReleaseSubView();

    /** 获取所属的父界面。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|SubView")
    ULyraUIView* GetOwningView() const { return OwningView.Get(); }

public:
    /** 子界面初始化时调用一次。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|SubView|Lifecycle")
    void OnInit();

    /** 子界面变为可见时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|SubView|Lifecycle")
    void OnOpen();

    /** 子界面绑定事件时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|SubView|Lifecycle")
    void OnAddListener();

    /** 子界面移除事件时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|SubView|Lifecycle")
    void OnRemoveListener();

    /** 子界面隐藏时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|SubView|Lifecycle")
    void OnClose();

    /** 子界面销毁时调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|SubView|Lifecycle")
    void OnRelease();

protected:
    /** 子界面销毁时执行释放回调。 */
    virtual void NativeDestruct() override;

private:
    /** 子界面所属的父 View。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<ULyraUIView> OwningView;

    /** 标记初始化回调已执行。 */
    bool bInitialized = false;

    /** 标记子界面是否打开。 */
    bool bSubViewOpen = false;

    /** 保证逻辑释放只发送一次。 */
    bool bReleased = false;
};
