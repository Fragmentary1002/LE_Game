#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LyraUIFrameworkEventSubsystem.generated.h"

/** UI 事件携带的轻量业务数据。 */
USTRUCT(BlueprintType)
struct LYRAUIFRAMEWORK_API FLyraUIFrameworkEventData
{
    GENERATED_BODY()

    /** 事件自定义对象数据。 */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI Event")
    TObjectPtr<UObject> ObjectValue = nullptr;

    /** 事件自定义文本数据。 */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI Event")
    FString StringValue;

    /** 事件自定义整数数据。 */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI Event")
    int32 IntValue = 0;

    /** 事件自定义浮点数据。 */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI Event")
    float FloatValue = 0.0f;
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FLyraUIFrameworkEventDelegate, const FLyraUIFrameworkEventData&, EventData);

/** 按本地玩家隔离分发基于 GameplayTag 的 UI 事件。 */
UCLASS()
class LYRAUIFRAMEWORK_API ULyraUIFrameworkEventSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** 将监听者的指定 UFUNCTION 注册到事件标签。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Event")
    bool AddListener(FGameplayTag EventTag, UObject* Listener, FName FunctionName);

    /** 移除监听者在该事件标签下的所有回调。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Event")
    void RemoveListener(FGameplayTag EventTag, UObject* Listener);

    /** 向当前本地玩家的事件监听者分发数据。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Event")
    void TriggerEvent(FGameplayTag EventTag, const FLyraUIFrameworkEventData& EventData);

    /** 子系统销毁时释放全部事件监听。 */
    virtual void Deinitialize() override;

private:
    /** 按标签保存的脚本事件监听列表。 */
    TMap<FGameplayTag, TArray<FLyraUIFrameworkEventDelegate>> Listeners;
};
