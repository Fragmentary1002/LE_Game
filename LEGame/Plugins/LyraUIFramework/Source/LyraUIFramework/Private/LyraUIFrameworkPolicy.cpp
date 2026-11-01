#include "LyraUIFrameworkPolicy.h"

#include "LyraUIFrameworkRootLayout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkPolicy)

/** 为每个本地玩家选择带有 Lyra 兼容 Layer 的框架布局。 */
TSubclassOf<UPrimaryGameLayout> ULyraUIFrameworkPolicy::GetLayoutWidgetClass(UCommonLocalPlayer* LocalPlayer)
{
    if (TSubclassOf<UPrimaryGameLayout> ConfiguredLayout = Super::GetLayoutWidgetClass(LocalPlayer))
    {
        return ConfiguredLayout;
    }
    return ULyraUIFrameworkRootLayout::StaticClass();
}
