#include <Helix/RenderPipelines/Standard/Overrides/Overrides.h>

namespace helix::rendering::universal
{
    Bloom::Bloom()
    {
        SetDisplayName("Bloom");
        AddParameter(threshold);
        AddParameter(intensity);
        AddParameter(scatter);
        AddParameter(clamp);
        AddParameter(tint);
        AddParameter(highQualityFiltering);
        AddParameter(downscale);
        AddParameter(maxIterations);
        AddParameter(dirtTexture);
        AddParameter(dirtIntensity);
    }

    Tonemapping::Tonemapping()
    {
        SetDisplayName("Tonemapping");
        AddParameter(mode);
    }

    ColorAdjustments::ColorAdjustments()
    {
        SetDisplayName("Color Adjustments");
        AddParameter(postExposure);
        AddParameter(contrast);
        AddParameter(colorFilter);
        AddParameter(hueShift);
        AddParameter(saturation);
    }

    bool ColorAdjustments::IsActive() const
    {
        Color filter = colorFilter.value();
        return postExposure.value() != 0.0f || contrast.value() != 0.0f || filter.r != 1.0f || filter.g != 1.0f || filter.b != 1.0f || filter.a != 1.0f ||
               hueShift.value() != 0.0f || saturation.value() != 0.0f;
    }
}
