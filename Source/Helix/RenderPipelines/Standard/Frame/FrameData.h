#pragma once

#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Pipeline/RenderStates.h>
#include <Helix/Engine/Texture/RenderTexture.h>
#include <Helix/RenderPipelines/Core/Context/ContextContainer.h>
#include <Helix/RenderPipelines/Core/RenderGraph/RenderGraph.h>
#include <Helix/RenderPipelines/Standard/Settings/PipelineSettings.h>

// The data of a camera's frame the renderer fills and its passes read (through the frame's ContextContainer).
namespace helix::rendering::universal
{
    class ScriptableRenderer;

    // The textures of the frame on the render graph.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalResourceData final : public ContextItem
    {
    public:
        HX_FUNCTION(Constructor) UniversalResourceData() = default;

        TextureHandle backBufferColor, backBufferDepth;
        TextureHandle cameraColor, cameraDepth;
        TextureHandle cameraDepthTexture, cameraNormalsTexture, cameraOpaqueTexture;
        TextureHandle afterPostProcessColor, internalColorLut, mainShadowsTexture;
        // Whether the passes draw into the back buffer now (else into the camera's textures).
        HX_FIELD() bool isActiveTargetBackBuffer = false;

        TextureHandle activeColorTexture() const { return isActiveTargetBackBuffer ? backBufferColor : cameraColor; }
        TextureHandle activeDepthTexture() const { return isActiveTargetBackBuffer ? backBufferDepth : cameraDepth; }

        HX_FUNCTION() void Reset() override;
        scripting::TypeId HelixType() const override;

        // The handles as C# packs them (by name: 0 backBufferColor ... 9 mainShadowsTexture, 10 and 11 the active ones).
        HX_FUNCTION(Internal) u64 Packed(i32 which) const;
        HX_FUNCTION(Internal) void SetPacked(i32 which, u64 packed);
    };

    // The camera of the frame and what the renderer made of it.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalCameraData final : public ContextItem
    {
    public:
        HX_FUNCTION(Constructor) UniversalCameraData() = default;

        HX_FIELD() engine::Camera* camera = nullptr;
        // The base camera of an overlay camera's stack (the camera itself here).
        HX_FIELD() engine::Camera* baseCamera = nullptr;
        HX_FIELD() CameraRenderType renderType = CameraRenderType::Base;
        HX_FIELD() engine::CameraType cameraType = engine::CameraType::Game;
        HX_FIELD() engine::RenderTexture* targetTexture = nullptr;
        engine::RenderTextureDescriptor cameraTargetDescriptor;
        HX_FIELD() f32 renderScale = 1;
        // The camera's size in pixels at the render scale.
        HX_FIELD() i32 scaledWidth = 0;
        HX_FIELD() i32 scaledHeight = 0;
        HX_FIELD() bool isDefaultViewport = true;
        HX_FIELD() bool isHdrEnabled = false;
        HX_FIELD() bool requiresDepthTexture = false;
        HX_FIELD() bool requiresOpaqueTexture = false;
        HX_FIELD() bool postProcessEnabled = false;
        HX_FIELD() bool postProcessingRequiresDepthTexture = false;
        // Whether the last camera of the stack draws into the camera's target at the end.
        HX_FIELD() bool resolveFinalTarget = true;
        HX_FIELD() f32 maxShadowDistance = 0;
        HX_FIELD() engine::SortingCriteria defaultOpaqueSortFlags = engine::SortingCriteria::CommonOpaque;
        HX_FIELD() Color backgroundColor;
        HX_FIELD() Vector3 worldSpaceCameraPos;
        HX_FIELD() engine::Transform* volumeTrigger = nullptr;
        HX_FIELD(Name = "volumeLayerMaskBits", Internal) i32 volumeLayerMask = 1;
        // The renderer that draws the frame.
        HX_FIELD() ScriptableRenderer* renderer = nullptr;

        HX_PROPERTY(ReadOnly) bool isGameCamera() const { return cameraType == engine::CameraType::Game; }
        HX_PROPERTY(ReadOnly) bool isSceneViewCamera() const { return cameraType == engine::CameraType::SceneView; }
        HX_PROPERTY(ReadOnly) bool isPreviewCamera() const { return cameraType == engine::CameraType::Preview; }

        HX_FUNCTION() Matrix4x4 GetViewMatrix(i32 viewIndex = 0) const;
        HX_FUNCTION() Matrix4x4 GetProjectionMatrix(i32 viewIndex = 0) const;
        // The projection as the device takes it, for the camera's own target (flipped when that is a texture), or for
        // a texture or the back buffer.
        HX_FUNCTION() Matrix4x4 GetGPUProjectionMatrix(i32 viewIndex = 0) const;
        HX_FUNCTION() Matrix4x4 GetGPUProjectionMatrix(bool renderIntoTexture, i32 viewIndex = 0) const;
        HX_FUNCTION() Matrix4x4 GetGPUProjectionMatrixNoJitter(i32 viewIndex = 0) const { return GetGPUProjectionMatrix(viewIndex); }
        HX_FUNCTION() Matrix4x4 GetGPUProjectionMatrixNoJitter(bool renderIntoTexture, i32 viewIndex = 0) const
        {
            return GetGPUProjectionMatrix(renderIntoTexture, viewIndex);
        }
        // Whether the camera's projection is drawn upside down (it draws into a texture: its own target or the
        // renderer's intermediate one).
        HX_FUNCTION() bool IsCameraProjectionMatrixFlipped() const { return targetTexture != nullptr || rendersIntoTexture; }
        bool rendersIntoTexture = false;

        HX_FUNCTION() void Reset() override;
        scripting::TypeId HelixType() const override;

        // The descriptor in its plain form for C# (written back by ApplyDescriptor).
        HX_FUNCTION(Internal) u64 DescriptorAddress();
        HX_FUNCTION(Internal) void ApplyDescriptor() { cameraTargetDescriptor = descriptorData_.ToDescriptor(); }

    private:
        engine::RenderTextureDescriptorData descriptorData_;
    };

    // The lights of the frame: the main light (an index into the visible lights, -1: none) and the additional ones.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalLightData final : public ContextItem
    {
    public:
        HX_FUNCTION(Constructor) UniversalLightData() = default;

        HX_FIELD() i32 mainLightIndex = -1;
        HX_FIELD() i32 additionalLightsCount = 0;
        HX_FIELD() i32 maxPerObjectAdditionalLightsCount = 0;
        HX_FIELD() bool shadeAdditionalLightsPerVertex = false;
        HX_FIELD() bool supportsAdditionalLights = false;
        Span<const engine::VisibleLight> visibleLights;

        HX_FUNCTION() void Reset() override;
        scripting::TypeId HelixType() const override;

        HX_FUNCTION(Internal) u64 VisibleLightsAddress() const { return u64(reinterpret_cast<uintptr_t>(visibleLights.Data())); }
        HX_FUNCTION(Internal) i32 VisibleLightCount() const { return i32(visibleLights.Size()); }
    };

    // The main light's shadows of the frame.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalShadowData final : public ContextItem
    {
    public:
        HX_FUNCTION(Constructor) UniversalShadowData() = default;

        HX_FIELD() bool supportsMainLightShadows = false;
        HX_FIELD() bool supportsSoftShadows = false;
        HX_FIELD() i32 mainLightShadowmapWidth = 0;
        HX_FIELD() i32 mainLightShadowmapHeight = 0;
        HX_FIELD() i32 mainLightShadowCascadesCount = 0;
        HX_FIELD() Vector3 mainLightShadowCascadesSplit;
        HX_FIELD() f32 mainLightShadowCascadeBorder = 0;
        HX_FIELD() i32 shadowmapDepthBufferBits = 16;
        // Per visible light its depth bias (x) and normal bias (y).
        HX_FIELD(Name = "biasArray", Internal) Array<Vector4> bias = Array<Vector4>();

        HX_FUNCTION() void Reset() override;
        scripting::TypeId HelixType() const override;
    };

    // The culling and drawing settings of the frame.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalRenderingData final : public ContextItem
    {
    public:
        HX_FUNCTION(Constructor) UniversalRenderingData() = default;

        engine::CullingResults cullResults;
        HX_FIELD() engine::PerObjectData perObjectData = engine::PerObjectData::None;
        HX_FIELD() RenderingMode renderingMode = RenderingMode::Forward;
        HX_FIELD() bool supportsDynamicBatching = false;
        HX_FIELD(Name = "opaqueLayerMaskBits", Internal) i32 opaqueLayerMask = -1;
        HX_FIELD(Name = "transparentLayerMaskBits", Internal) i32 transparentLayerMask = -1;

        HX_FUNCTION() void Reset() override;
        scripting::TypeId HelixType() const override;

        HX_FUNCTION(Internal) u64 CullResultsId() const { return u64(reinterpret_cast<uintptr_t>(cullResults.data)); }
    };

    // The post processing of the frame.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalPostProcessingData final : public ContextItem
    {
    public:
        HX_FUNCTION(Constructor) UniversalPostProcessingData() = default;

        HX_FIELD() bool isEnabled = false;
        HX_FIELD() ColorGradingMode gradingMode = ColorGradingMode::LowDynamicRange;
        HX_FIELD() i32 lutSize = 32;
        HX_FIELD() bool useFastSRGBLinearConversion = false;

        HX_FUNCTION() void Reset() override;
        scripting::TypeId HelixType() const override;
    };
}
