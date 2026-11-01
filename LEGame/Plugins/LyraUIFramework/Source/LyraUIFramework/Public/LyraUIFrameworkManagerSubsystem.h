#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LyraUIFrameworkManagerSubsystem.generated.h"

class ULyraUIView;
class ULyraUIViewHandle;
struct FLyraUIConfigRow;
struct FSubsystemCollectionBase;
enum class EAsyncWidgetLayerState : uint8;

/** 按本地玩家隔离管理 UI 配置、加载、实例和关闭流程。 */
UCLASS()
class LYRAUIFRAMEWORK_API ULyraUIFrameworkManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** 根据 UIName 读取配置并异步打开界面，立即返回可监听结果的句柄。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    ULyraUIViewHandle* OpenView(FName UIName, UObject* OpenContext = nullptr);

    /** 关闭指定打开句柄对应的界面。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void CloseView(ULyraUIViewHandle* Handle);

    /** 根据界面实例查找并关闭对应句柄。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void CloseViewByWidget(ULyraUIView* View);

    /** 关闭当前玩家的所有非常驻界面。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI")
    void CloseAll(bool bCloseResidentViews = false);

    /** 查找 UI 配置；未配置或配置表不可用时返回空。 */
    const FLyraUIConfigRow* FindConfig(FName UIName) const;

    /** 子系统销毁时取消未完成的界面加载。 */
    virtual void Deinitialize() override;

private:
    /** 根据异步加载状态更新句柄与界面生命周期。 */
    void HandleAsyncOpenState(TWeakObjectPtr<ULyraUIViewHandle> WeakHandle, EAsyncWidgetLayerState AsyncState, ULyraUIView* View);

    /** 将失败状态写入句柄并通知调用方。 */
    void FailOpen(ULyraUIViewHandle* Handle);

    /** 正在加载或已打开的界面句柄。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULyraUIViewHandle>> ActiveHandles;
};
