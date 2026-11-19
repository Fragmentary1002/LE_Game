#include "LEUIFrameWorkAdapterLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LEUIFrameWorkAdapterLibrary)

/** 依据当前视口比例和设计比例计算黑边留白。 */
FMargin ULEUIFrameWorkAdapterLibrary::CalculateBlackBarPadding(FVector2D ViewportSize, FVector2D DesignResolution, ELEUIFrameWorkBlackType BlackType)
{
    if (BlackType == ELEUIFrameWorkBlackType::None || ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f ||
        DesignResolution.X <= 0.0f || DesignResolution.Y <= 0.0f)
    {
        return FMargin(0.0f);
    }

    const float ViewportAspect = ViewportSize.X / ViewportSize.Y;
    const float DesignAspect = DesignResolution.X / DesignResolution.Y;
    if (BlackType == ELEUIFrameWorkBlackType::Height)
    {
        const float HorizontalPadding = (ViewportSize.X - (ViewportSize.Y * DesignAspect)) * 0.5f;
        return FMargin(HorizontalPadding, 0.0f, HorizontalPadding, 0.0f);
    }

    if (BlackType == ELEUIFrameWorkBlackType::Width)
    {
        const float VerticalPadding = (ViewportSize.Y - (ViewportSize.X / DesignAspect)) * 0.5f;
        return FMargin(0.0f, VerticalPadding, 0.0f, VerticalPadding);
    }

    if (ViewportAspect > DesignAspect)
    {
        const float HorizontalPadding = (ViewportSize.X - (ViewportSize.Y * DesignAspect)) * 0.5f;
        return FMargin(HorizontalPadding, 0.0f, HorizontalPadding, 0.0f);
    }

    if (ViewportAspect < DesignAspect)
    {
        const float VerticalPadding = (ViewportSize.Y - (ViewportSize.X / DesignAspect)) * 0.5f;
        return FMargin(0.0f, VerticalPadding, 0.0f, VerticalPadding);
    }

    return FMargin(0.0f);
}
