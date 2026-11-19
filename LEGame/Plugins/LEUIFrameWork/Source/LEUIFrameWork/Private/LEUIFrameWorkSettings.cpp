#include "LEUIFrameWorkSettings.h"

#include "LEUIFrameWorkTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEUIFrameWorkSettings)

/** 提供插件自带的默认 Layer，无需项目重复配置。 */
ULEUIFrameWorkSettings::ULEUIFrameWorkSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SceneLayer = LEUIFrameWorkTags::Scene;
    BackgroundLayer = LEUIFrameWorkTags::Background;
    NormalLayer = LEUIFrameWorkTags::Normal;
    InfoLayer = LEUIFrameWorkTags::Info;
    TopLayer = LEUIFrameWorkTags::Top;
    TipLayer = LEUIFrameWorkTags::Tip;
    ModalLayer = LEUIFrameWorkTags::Modal;
}
