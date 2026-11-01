#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LyraUIFrameworkManagerSubsystem.generated.h"

class ULyraUIView;
class ULyraUIViewHandle;
class ULyraUIFrameworkLayerHost;
struct FLyraUIConfigRow;
struct FSubsystemCollectionBase;

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
    /** 将失败状态写入句柄并通知调用方。 */
    void FailOpen(ULyraUIViewHandle* Handle);

    /** 恢复异步加载前挂起的玩家输入。 */
    void ResumeInputForHandle(ULyraUIViewHandle* Handle);

    /** 获取或在 Lyra RootLayout 中创建指定 Layer 的持久宿主。 */
    ULyraUIFrameworkLayerHost* GetOrCreateLayerHost(FGameplayTag LayerTag);

    /** 正在加载或已打开的界面句柄。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<ULyraUIViewHandle>> ActiveHandles;

    /** 每个 Lyra Layer 对应的内部 View 宿主。 */
    UPROPERTY(Transient)
    TMap<FGameplayTag, TObjectPtr<ULyraUIFrameworkLayerHost>> LayerHosts;
};
