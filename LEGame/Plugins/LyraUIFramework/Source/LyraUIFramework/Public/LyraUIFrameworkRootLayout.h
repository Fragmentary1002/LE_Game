#pragma once

#include "CoreMinimal.h"
#include "PrimaryGameLayout.h"
#include "LyraUIFrameworkRootLayout.generated.h"

class UCanvasPanel;

/** 从项目设置创建标准 Layer；自定义设计器布局应独立继承 UPrimaryGameLayout。 */
UCLASS(NotBlueprintable)
class LYRAUIFRAMEWORK_API ULyraUIFrameworkRootLayout : public UPrimaryGameLayout
{
    GENERATED_BODY()

public:
    /** 转交 CommonGame RootLayout 初始化参数。 */
    ULyraUIFrameworkRootLayout(const FObjectInitializer& ObjectInitializer);

protected:
    /** 创建框架标准层和 Lyra 兼容层并向 RootLayout 注册。 */
    virtual void NativeOnInitialized() override;

private:
    /** 运行时承载所有 Layer Stack 的全屏 Canvas。 */
    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> LayerCanvas;
};
