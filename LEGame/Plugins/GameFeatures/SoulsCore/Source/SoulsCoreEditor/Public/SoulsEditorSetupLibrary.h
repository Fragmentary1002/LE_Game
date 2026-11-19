#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoulsEditorSetupLibrary.generated.h"

class UBlueprint;
class UWidgetBlueprint;
class UUserWidget;

/** 将 Python 未导出的 Widget 蓝图编辑能力限定在编辑器模块内。 */
UCLASS()
class SOULSCOREEDITOR_API USoulsEditorSetupLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 为魂类 HUD 蓝图建立可在设计器中调整的控件树。
    UFUNCTION(BlueprintCallable, Category = "Souls|Editor")
    static bool BuildHUDWidgetTree(UWidgetBlueprint* Blueprint);

    // 编译蓝图并确认生成类有效，失败时阻止保存完成标记。
    UFUNCTION(BlueprintCallable, Category = "Souls|Editor")
    static bool CompileBlueprintForSetup(UBlueprint* Blueprint);

    // 使用标准 Lyra AddWidgets Action 配置魂类 HUD 的游戏层入口。
    UFUNCTION(BlueprintCallable, Category = "Souls|Editor")
    static UObject* CreateHUDAction(UObject* ActionSet, TSubclassOf<UUserWidget> LayoutClass);

    // 为指定关卡写入 Lyra Experience、GameMode 和单人 PIE 设置。
    UFUNCTION(BlueprintCallable, Category = "Souls|Editor")
    static bool ConfigureMap(UWorld* World, TSubclassOf<UObject> ExperienceClass);
};
