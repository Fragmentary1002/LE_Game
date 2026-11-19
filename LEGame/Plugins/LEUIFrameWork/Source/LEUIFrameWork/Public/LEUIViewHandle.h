#pragma once

#include "CoreMinimal.h"
#include "LEUIFrameWorkTypes.h"
#include "LEUIViewHandle.generated.h"

struct FStreamableHandle;
class ULEUIFrameWorkManagerSubsystem;
class ULEUIView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLEUIViewHandleEvent, ULEUIView*, View);

/** 一次 OpenView 操作的句柄，负责暴露异步结果并支持关闭指定实例。 */
UCLASS(BlueprintType)
class LEUIFRAMEWORK_API ULEUIViewHandle : public UObject
{
    GENERATED_BODY()

public:
    /** 获取当前打开状态。 */
    UFUNCTION(BlueprintPure, Category = "LE UI")
    ELEUIFrameWorkViewState GetState() const { return State; }

    /** 获取该句柄对应的 UI 标识。 */
    UFUNCTION(BlueprintPure, Category = "LE UI")
    FName GetUIName() const { return UIName; }

    /** 获取实际创建的界面实例。 */
    UFUNCTION(BlueprintPure, Category = "LE UI")
    ULEUIView* GetView() const { return View; }

    /** 关闭该句柄对应的界面实例。 */
    UFUNCTION(BlueprintCallable, Category = "LE UI")
    void Close();

    /** 异步打开成功后触发。 */
    UPROPERTY(BlueprintAssignable, Category = "LE UI")
    FLEUIViewHandleEvent OnOpened;

    /** 界面关闭后触发。 */
    UPROPERTY(BlueprintAssignable, Category = "LE UI")
    FLEUIViewHandleEvent OnClosed;

    /** 打开失败或资源加载取消后触发。 */
    UPROPERTY(BlueprintAssignable, Category = "LE UI")
    FLEUIViewHandleEvent OnFailed;

private:
    /** UI Manager 初始化句柄数据。 */
    void Initialize(ULEUIFrameWorkManagerSubsystem* InManager, FName InUIName, const FLEUIConfigRow& InConfig, UObject* InOpenContext);

    /** UI Manager 设置异步加载句柄。 */
    void SetStreamableHandle(TSharedPtr<FStreamableHandle> InHandle);

    /** UI Manager 设置实际 Widget 实例。 */
    void SetView(ULEUIView* InView);

    /** UI Manager 推进句柄状态。 */
    void SetState(ELEUIFrameWorkViewState InState);

    /** 所属管理器。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<ULEUIFrameWorkManagerSubsystem> Manager;

    /** UI 配置标识。 */
    UPROPERTY(Transient)
    FName UIName;

    /** 打开时使用的配置快照。 */
    UPROPERTY(Transient)
    FLEUIConfigRow Config;

    /** 打开时传入的业务对象。 */
    UPROPERTY(Transient)
    TObjectPtr<UObject> OpenContext;

    /** 实际创建的 Widget。 */
    UPROPERTY(Transient)
    TObjectPtr<ULEUIView> View = nullptr;

    /** 当前状态。 */
    UPROPERTY(Transient)
    ELEUIFrameWorkViewState State = ELEUIFrameWorkViewState::Unloaded;

    /** 异步加载句柄。 */
    TSharedPtr<FStreamableHandle> StreamableHandle;

    /** 异步加载期间挂起玩家输入时使用的 CommonUI token。 */
    FName InputSuspendToken;

    friend class ULEUIFrameWorkManagerSubsystem;
};
