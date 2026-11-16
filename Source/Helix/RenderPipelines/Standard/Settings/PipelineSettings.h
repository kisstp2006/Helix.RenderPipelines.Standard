#pragma once

#include <Helix/Core/Basics/Types.h>
#include <Helix/Scripting/Bindings/Tags.h>

// The settings of the Standard pipeline as the asset, the renderer, the cameras and the passes name them.
namespace helix::rendering::universal
{
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class ShadowQuality : i32
    {
        Disabled = 0,
        HardShadows = 1,
        SoftShadows = 2,
    };

    // The size of a shadow map in texels.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class ShadowResolution : i32
    {
        _256 = 256,
        _512 = 512,
        _1024 = 1024,
        _2048 = 2048,
        _4096 = 4096,
        _8192 = 8192,
    };

    // The filtering of soft shadows (a light's own setting, or the asset's).
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class SoftShadowQuality : i32
    {
        UsePipelineSettings = 0,
        Low = 1,
        Medium = 2,
        High = 3,
    };

    // The samples per pixel of the camera's color.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class MsaaQuality : i32
    {
        Disabled = 1,
        _2x = 2,
        _4x = 4,
        _8x = 8,
    };

    // How the opaque texture is made smaller.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class Downsampling : i32
    {
        None = 0,
        _2xBilinear = 1,
        _4xBox = 2,
        _4xBilinear = 3,
    };

    // How the main light and the additional lights are evaluated.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class LightRenderingMode : i32
    {
        Disabled = 0,
        PerVertex = 2,
        PerPixel = 1,
    };

    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class ColorGradingMode : i32
    {
        LowDynamicRange = 0,
        HighDynamicRange = 1,
    };

    // Whether render targets are stored or discarded after a pass (on tile based GPUs).
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class StoreActionsOptimization : i32
    {
        Auto = 0,
        Discard = 1,
        Store = 2,
    };

    // The rendering path of a renderer.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class RenderingMode : i32
    {
        Forward = 0,
        Deferred = 1,
        ForwardPlus = 2,
    };

    // A camera that renders the scene (Base) or draws over another camera's result (Overlay).
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class CameraRenderType : i32
    {
        Base = 0,
        Overlay = 1,
    };

    // A camera's own value of a setting, or the asset's.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class CameraOverrideOption : i32
    {
        Off = 0,
        On = 1,
        UsePipelineSettings = 2,
    };

    // Where in the frame a renderer pass runs; passes sort by it.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class RenderPassEvent : i32
    {
        BeforeRendering = 0,
        BeforeRenderingShadows = 50,
        AfterRenderingShadows = 100,
        BeforeRenderingPrePasses = 150,
        AfterRenderingPrePasses = 200,
        BeforeRenderingGbuffer = 210,
        AfterRenderingGbuffer = 220,
        BeforeRenderingDeferredLights = 230,
        AfterRenderingDeferredLights = 240,
        BeforeRenderingOpaques = 250,
        AfterRenderingOpaques = 300,
        BeforeRenderingSkybox = 350,
        AfterRenderingSkybox = 400,
        BeforeRenderingTransparents = 450,
        AfterRenderingTransparents = 500,
        BeforeRenderingPostProcessing = 550,
        AfterRenderingPostProcessing = 600,
        AfterRendering = 1000,
    };

    // The precision of an HDR camera color: 32 bits (R11G11B10) or 64 bits (R16G16B16A16) per pixel.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class HDRColorBufferPrecision : i32
    {
        _32Bits = 0,
        _64Bits = 1,
    };

    // How a camera rendered at a render scale below 1 is scaled up.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class UpscalingFilterSelection : i32
    {
        Auto = 0,
        Linear = 1,
        Point = 2,
        FSR = 3,
        STP = 4,
    };

    // When a camera's volume stack is blended: every frame, or when the script asks.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class VolumeFrameworkUpdateMode : i32
    {
        EveryFrame = 0,
        ViaScripting = 1,
        UsePipelineSettings = 2,
    };

    // Whether the renderer always draws into an intermediate texture, or only when something needs one.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class IntermediateTextureMode : i32
    {
        Auto = 0,
        Always = 1,
    };

    // When the camera's depth is copied into the depth texture.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class CopyDepthMode : i32
    {
        AfterOpaques = 0,
        AfterTransparents = 1,
        ForcePrepass = 2,
    };

    // The format of a depth buffer or texture (the graphics format's number; Default: the platform's).
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal")
    enum class DepthFormat : i32
    {
        Default = 0,
        Depth_16 = 90,
        Depth_24 = 91,
        Depth_32 = 93,
        Depth_24_Stencil_8 = 92,
        Depth_32_Stencil_8 = 94,
    };

    // The camera textures a renderer pass reads.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal", Attributes = "[System.Flags]")
    enum class ScriptableRenderPassInput : i32
    {
        None = 0,
        Depth = 1,
        Normal = 2,
        Color = 4,
        Motion = 8,
    };
}
