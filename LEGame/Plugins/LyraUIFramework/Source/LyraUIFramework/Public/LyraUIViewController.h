#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LyraUIViewController.generated.h"

class ULyraUIView;

/** 可复用的界面业务控制器；实例和资源生命周期由 View 拥有。 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LYRAUIFRAMEWORK_API ULyraUIViewController : public UObject
{
    GENERATED_BODY()

public:
    /** 注入所属 View 和打开参数，随后执行初始化事件。 */
    void InitializeController(ULyraUIView* InView, UObject* Context);

    /** 返回所属界面，关闭后该弱引用不延长 View 生命周期。 */
    UFUNCTION(BlueprintPure, Category = "Lyra UI|Controller")
    ULyraUIView* GetView() const;

    /** 使用 View 所属世界支持蓝图定时器等调用。 */
    virtual UWorld* GetWorld() const override;

    /** 初始化控制器的数据模型。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Controller")
    void OnInit(UObject* Context);

    /** 界面打开后刷新业务。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Controller")
    void OnOpen();

    /** 覆盖界面时暂停业务。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Controller")
    void OnPause();

    /** 从覆盖恢复时继续业务。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Controller")
    void OnResume();

    /** 关闭前停止业务。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Controller")
    void OnClose();

    /** 最终移除前释放业务资源。 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lyra UI|Controller")
    void OnRelease();

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<ULyraUIView> View;
};
