#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LyraUIControlBinding.generated.h"

class UButton;

/** 将 UMG 控件事件绑定到拥有者的 UFUNCTION，并统一管理解绑。 */
UCLASS(BlueprintType)
class LYRAUIFRAMEWORK_API ULyraUIControlBinding : public UObject
{
    GENERATED_BODY()

public:
    /** 将 Button 点击事件绑定到目标对象函数。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Binding")
    bool BindButton(UButton* Button, UObject* Target, FName FunctionName);

    /** 解绑当前对象管理的全部控件事件。 */
    UFUNCTION(BlueprintCallable, Category = "Lyra UI|Binding")
    void UnbindAll();

    /** 对象销毁前移除所有控件委托。 */
    virtual void BeginDestroy() override;

private:
    /** 记录已绑定的按钮，便于生命周期结束时清理委托。 */
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<UButton>> BoundButtons;

    /** 记录按钮对应的回调对象，便于精确移除委托。 */
    TArray<TWeakObjectPtr<UObject>> BoundTargets;

    /** 记录按钮对应的回调函数名。 */
    TArray<FName> BoundFunctionNames;
};
