#pragma once

#include <Helix/RenderPipelines/Core/Volume/VolumeComponent.h>

// The pipeline's post processing volume components (the reference's overrides): their parameters are the C# classes'
// properties; the forward renderer's post process pass reads the stack's blended ones.
namespace helix::rendering::universal
{
    // The resolution bloom starts at.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class BloomDownscaleMode : i32
    {
        Half = 0,
        Quarter = 1,
    };

    HX_HANDLE(CSharp = "HelixEngine.Rendering.Universal.DownscaleParameter")
    class DownscaleParameter : public VolumeParameterOf<BloomDownscaleMode>
    {
    public:
        using VolumeParameterOf::VolumeParameterOf;
    };

    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class TonemappingMode : i32
    {
        None = 0,
        Neutral = 1,
        ACES = 2,
    };

    HX_HANDLE(CSharp = "HelixEngine.Rendering.Universal.TonemappingModeParameter")
    class TonemappingModeParameter : public VolumeParameterOf<TonemappingMode>
    {
    public:
        using VolumeParameterOf::VolumeParameterOf;
    };

    // Light bleeding from the bright parts of the image: the parts above the threshold blurred over a chain of halved
    // textures and added back by intensity and tint, with an optional dirt texture over the screen.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Sealed)
    class Bloom final : public VolumeComponent
    {
    public:
        Bloom();

        HX_FIELD() MinFloatParameter threshold{0.9f, 0.0f};
        HX_FIELD() MinFloatParameter intensity{0.0f, 0.0f};
        HX_FIELD() ClampedFloatParameter scatter{0.7f, 0.0f, 1.0f};
        HX_FIELD() MinFloatParameter clamp{65472.0f, 0.0f};
        HX_FIELD() ColorParameter tint{Color(1, 1, 1, 1), false, false, true};
        HX_FIELD() BoolParameter highQualityFiltering{false};
        HX_FIELD() DownscaleParameter downscale{BloomDownscaleMode::Half};
        HX_FIELD() ClampedIntParameter maxIterations{6, 2, 8};
        HX_FIELD() TextureParameter dirtTexture;
        HX_FIELD() MinFloatParameter dirtIntensity{0.0f, 0.0f};

        HX_FUNCTION() bool IsActive() const { return intensity.value() > 0.0f; }
    };

    // The mapping of HDR colors into the display's range.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Sealed)
    class Tonemapping final : public VolumeComponent
    {
    public:
        Tonemapping();

        HX_FIELD() TonemappingModeParameter mode{TonemappingMode::None};

        HX_FUNCTION() bool IsActive() const { return mode.value() != TonemappingMode::None; }
    };

    // The overall tone, contrast, color, hue and saturation of the image.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Sealed)
    class ColorAdjustments final : public VolumeComponent
    {
    public:
        ColorAdjustments();

        // In EV: the image times 2^postExposure before grading.
        HX_FIELD() FloatParameter postExposure{0.0f};
        HX_FIELD() ClampedFloatParameter contrast{0.0f, -100.0f, 100.0f};
        HX_FIELD() ColorParameter colorFilter{Color(1, 1, 1, 1), true, false, true};
        HX_FIELD() ClampedFloatParameter hueShift{0.0f, -180.0f, 180.0f};
        HX_FIELD() ClampedFloatParameter saturation{0.0f, -100.0f, 100.0f};

        HX_FUNCTION() bool IsActive() const;
    };
}
