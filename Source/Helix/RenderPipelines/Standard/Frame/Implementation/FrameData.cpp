#include <Helix/RenderPipelines/Standard/Frame/FrameData.h>

#include <Helix/Engine/Rendering/GL.h>

namespace helix::rendering::universal
{
    namespace
    {
        u64 Pack(TextureHandle h) { return u64(h.index) | (u64(h.execution) << 32); }
        TextureHandle Unpack(u64 packed) { return TextureHandle{u32(packed), u32(packed >> 32)}; }
    }

    // ---------------------------------------------------------------------------------------------- resources

    void UniversalResourceData::Reset()
    {
        backBufferColor = backBufferDepth = cameraColor = cameraDepth = TextureHandle();
        cameraDepthTexture = cameraNormalsTexture = cameraOpaqueTexture = TextureHandle();
        afterPostProcessColor = internalColorLut = mainShadowsTexture = TextureHandle();
        isActiveTargetBackBuffer = false;
    }

    scripting::TypeId UniversalResourceData::HelixType() const { return scripting::TypeOf<UniversalResourceData>(); }

    u64 UniversalResourceData::Packed(i32 which) const
    {
        const TextureHandle all[] = {backBufferColor,      backBufferDepth,     cameraColor,           cameraDepth,     cameraDepthTexture,
                                     cameraNormalsTexture, cameraOpaqueTexture, afterPostProcessColor, internalColorLut, mainShadowsTexture,
                                     activeColorTexture(), activeDepthTexture()};
        return which >= 0 && which < i32(sizeof(all) / sizeof(all[0])) ? Pack(all[which]) : 0;
    }

    void UniversalResourceData::SetPacked(i32 which, u64 packed)
    {
        TextureHandle* all[] = {&backBufferColor,      &backBufferDepth,     &cameraColor,           &cameraDepth,      &cameraDepthTexture,
                                &cameraNormalsTexture, &cameraOpaqueTexture, &afterPostProcessColor, &internalColorLut, &mainShadowsTexture};
        if (which >= 0 && which < i32(sizeof(all) / sizeof(all[0])))
            *all[which] = Unpack(packed);
    }

    // ---------------------------------------------------------------------------------------------- the camera

    void UniversalCameraData::Reset()
    {
        camera = baseCamera = nullptr;
        renderType = CameraRenderType::Base;
        cameraType = engine::CameraType::Game;
        targetTexture = nullptr;
        cameraTargetDescriptor = engine::RenderTextureDescriptor();
        renderScale = 1;
        scaledWidth = scaledHeight = 0;
        isDefaultViewport = true;
        isHdrEnabled = requiresDepthTexture = requiresOpaqueTexture = postProcessEnabled = postProcessingRequiresDepthTexture = false;
        resolveFinalTarget = true;
        maxShadowDistance = 0;
        defaultOpaqueSortFlags = engine::SortingCriteria::CommonOpaque;
        backgroundColor = Color();
        worldSpaceCameraPos = Vector3();
        volumeTrigger = nullptr;
        volumeLayerMask = 1;
        renderer = nullptr;
        rendersIntoTexture = false;
    }

    scripting::TypeId UniversalCameraData::HelixType() const { return scripting::TypeOf<UniversalCameraData>(); }

    Matrix4x4 UniversalCameraData::GetViewMatrix(i32) const { return camera ? camera->worldToCameraMatrix() : Matrix4x4::identity; }

    Matrix4x4 UniversalCameraData::GetProjectionMatrix(i32) const { return camera ? camera->projectionMatrix() : Matrix4x4::identity; }

    Matrix4x4 UniversalCameraData::GetGPUProjectionMatrix(i32 viewIndex) const { return GetGPUProjectionMatrix(IsCameraProjectionMatrixFlipped(), viewIndex); }

    Matrix4x4 UniversalCameraData::GetGPUProjectionMatrix(bool renderIntoTexture, i32 viewIndex) const
    {
        return engine::GL::GetGPUProjectionMatrix(GetProjectionMatrix(viewIndex), renderIntoTexture);
    }

    u64 UniversalCameraData::DescriptorAddress()
    {
        descriptorData_ = engine::RenderTextureDescriptorData::From(cameraTargetDescriptor);
        return u64(reinterpret_cast<uintptr_t>(&descriptorData_));
    }

    // ---------------------------------------------------------------------------------------------- the rest

    void UniversalLightData::Reset()
    {
        mainLightIndex = -1;
        additionalLightsCount = maxPerObjectAdditionalLightsCount = 0;
        shadeAdditionalLightsPerVertex = supportsAdditionalLights = false;
        visibleLights = Span<const engine::VisibleLight>();
    }

    scripting::TypeId UniversalLightData::HelixType() const { return scripting::TypeOf<UniversalLightData>(); }

    void UniversalShadowData::Reset()
    {
        supportsMainLightShadows = supportsSoftShadows = false;
        mainLightShadowmapWidth = mainLightShadowmapHeight = mainLightShadowCascadesCount = 0;
        mainLightShadowCascadesSplit = Vector3();
        mainLightShadowCascadeBorder = 0;
        shadowmapDepthBufferBits = 16;
        bias.Clear();
    }

    scripting::TypeId UniversalShadowData::HelixType() const { return scripting::TypeOf<UniversalShadowData>(); }

    void UniversalRenderingData::Reset()
    {
        cullResults = engine::CullingResults();
        perObjectData = engine::PerObjectData::None;
        renderingMode = RenderingMode::Forward;
        supportsDynamicBatching = false;
        opaqueLayerMask = transparentLayerMask = -1;
    }

    scripting::TypeId UniversalRenderingData::HelixType() const { return scripting::TypeOf<UniversalRenderingData>(); }

    void UniversalPostProcessingData::Reset()
    {
        isEnabled = false;
        gradingMode = ColorGradingMode::LowDynamicRange;
        lutSize = 32;
        useFastSRGBLinearConversion = false;
    }

    scripting::TypeId UniversalPostProcessingData::HelixType() const { return scripting::TypeOf<UniversalPostProcessingData>(); }
}
