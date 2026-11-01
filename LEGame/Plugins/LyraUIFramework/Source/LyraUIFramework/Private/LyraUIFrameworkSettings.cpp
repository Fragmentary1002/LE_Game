#include "LyraUIFrameworkSettings.h"

#include "LyraUIFrameworkTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUIFrameworkSettings)

/** 提供插件自带的默认 Layer，无需项目重复配置。 */
ULyraUIFrameworkSettings::ULyraUIFrameworkSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SceneLayer = LyraUIFrameworkTags::Scene;
    BackgroundLayer = LyraUIFrameworkTags::Background;
    NormalLayer = LyraUIFrameworkTags::Normal;
    InfoLayer = LyraUIFrameworkTags::Info;
    TopLayer = LyraUIFrameworkTags::Top;
    TipLayer = LyraUIFrameworkTags::Tip;
    ModalLayer = LyraUIFrameworkTags::Modal;
}
