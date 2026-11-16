#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipeline.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Rendering/RenderSettings.h>
#include <Helix/RHI/Device/Device.h>
#include <Helix/RenderPipelines/Core/Textures/RTHandle.h>
#include <Helix/RenderPipelines/Core/Utilities/Blitter.h>
#include <Helix/RenderPipelines/Core/Volume/Volume.h>
#include <Helix/RenderPipelines/Standard/Components/AdditionalData.h>
#include <Helix/RenderPipelines/Standard/Passes/PostProcessPass.h>
#include <Helix/Scripting/Bindings/Interop.h>

#include <cmath>

namespace helix::rendering::universal
{
    namespace
    {
        LogChannel gStandardLog{"Standard"};
    }

    // ---------------------------------------------------------------------------------------------- the asset

    namespace
    {
        engine::PPtr<VolumeProfile>& GlobalProfile()
        {
            static engine::PPtr<VolumeProfile> p;
            return p;
        }
    }

    UniversalRenderPipelineAsset::~UniversalRenderPipelineAsset()
    {
        // The default materials are the asset's own.
        for (engine::Material* m : {defaultMaterial_.Get(), defaultParticleMaterial_.Get()})
            if (m)
                engine::Object::DestroyImmediate(m);
    }

    void UniversalRenderPipelineAsset::Renderers::Clear()
    {
        for (u64& h : handles)
            scripting::ReleaseWrapper(h);
        handles.Clear();
        list.Clear();
    }

    void UniversalRenderPipelineAsset::Renderers::Set(u32 index, ScriptableRenderer* renderer, u64 handle)
    {
        while (list.Size() <= index)
            list.Emplace();
        while (handles.Size() <= index)
            handles.Push(0);
        list[index].reset(renderer);
        scripting::ReleaseWrapper(handles[index]);
        handles[index] = handle;
    }

    VolumeProfile* UniversalRenderPipelineAsset::globalVolumeProfile() { return GlobalProfile().Get(); }

    void UniversalRenderPipelineAsset::SetGlobalVolumeProfile(VolumeProfile* profile) { GlobalProfile() = engine::PPtr<VolumeProfile>(profile); }

    void UniversalRenderPipelineAsset::OnAfterDeserialize()
    {
        if (assetVersion_ >= k_AssetVersion)
            return;
        // Version 1 kept one renderer data.
        if (assetVersion_ <= 1 && rendererData_.Empty() && legacyRendererData_.Get())
        {
            rendererData_.Push(legacyRendererData_);
            defaultRendererIndex_ = 0;
        }
        // Before version 6 the cascades were an option: none, two or four.
        if (assetVersion_ < 6)
            shadowCascadeCount_ = legacyShadowCascades_ == 1 ? 2 : legacyShadowCascades_ == 2 ? 4 : 1;
        assetPreviousVersion_ = assetVersion_;
        assetVersion_ = k_AssetVersion;
        renderers_.Clear();
    }

    UniversalRenderPipelineAsset* UniversalRenderPipelineAsset::Create(ScriptableRendererData* rendererData)
    {
        auto* asset = static_cast<UniversalRenderPipelineAsset*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<UniversalRenderPipelineAsset>()));
        if (!asset)
            return nullptr;
        if (!rendererData)
            rendererData = static_cast<UniversalRendererData*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<UniversalRendererData>()));
        ScriptableRendererData* list[] = {rendererData};
        asset->SetRendererDataList(Span<ScriptableRendererData* const>(list, rendererData ? 1 : 0));
        return asset;
    }

    Array<ScriptableRendererData*> UniversalRenderPipelineAsset::rendererDataList() const
    {
        Array<ScriptableRendererData*> out;
        for (const engine::PPtr<ScriptableRendererData>& d : rendererData_)
            out.Push(d.Get());
        return out;
    }

    void UniversalRenderPipelineAsset::SetRendererDataList(Span<ScriptableRendererData* const> list, i32 defaultIndex)
    {
        rendererData_.Clear();
        for (ScriptableRendererData* d : list)
            rendererData_.Push(engine::PPtr<ScriptableRendererData>(d));
        defaultRendererIndex_ = defaultIndex >= 0 && defaultIndex < i32(list.Size()) ? defaultIndex : 0;
        renderers_.Clear();
    }

    ScriptableRenderer* UniversalRenderPipelineAsset::GetRenderer(i32 index)
    {
        if (index < 0)
            index = defaultRendererIndex_;
        if (index < 0 || index >= i32(rendererData_.Size()))
            return nullptr;
        ScriptableRendererData* data = rendererData_[u32(index)].Get();
        if (!data)
            return nullptr;
        ScriptableRenderer* r = u32(index) < renderers_.list.Size() ? renderers_.list[u32(index)].get() : nullptr;
        if (!r || r->data() != data || r->dataVersion() != data->version())
        {
            renderers_.Set(u32(index), nullptr, 0);
            u64 handle = 0;
            r = data->CallCreateRenderer(handle);
            renderers_.Set(u32(index), r, handle);
        }
        return r;
    }

    Array<ScriptableRenderer*> UniversalRenderPipelineAsset::renderers()
    {
        Array<ScriptableRenderer*> out;
        for (i32 i = 0; i < i32(rendererData_.Size()); i++)
            out.Push(GetRenderer(i));
        return out;
    }

    void UniversalRenderPipelineAsset::SetMsaaSampleCount(i32 value)
    {
        msaa_ = value >= 8 ? MsaaQuality::_8x : value >= 4 ? MsaaQuality::_4x : value >= 2 ? MsaaQuality::_2x : MsaaQuality::Disabled;
    }

    void UniversalRenderPipelineAsset::SetRenderScale(f32 value)
    {
        renderScale_ = mathf::Clamp(value, UniversalRenderPipeline::minRenderScale, UniversalRenderPipeline::maxRenderScale);
    }

    void UniversalRenderPipelineAsset::SetMainLightShadowmapResolution(i32 value)
    {
        i32 r = 256;
        while (r < value && r < 8192)
            r *= 2;
        mainLightShadowmapResolution_ = ShadowResolution(r);
    }

    void UniversalRenderPipelineAsset::SetMaxAdditionalLightsCount(i32 value)
    {
        additionalLightsPerObjectLimit_ = mathf::Clamp(value, 0, UniversalRenderPipeline::maxPerObjectLights);
    }

    void UniversalRenderPipelineAsset::SetShadowDistance(f32 value) { shadowDistance_ = Max(0.0f, value); }

    void UniversalRenderPipelineAsset::SetShadowCascadeCount(i32 value) { shadowCascadeCount_ = mathf::Clamp(value, 1, 4); }

    void UniversalRenderPipelineAsset::SetShadowDepthBias(f32 value) { shadowDepthBias_ = mathf::Clamp(value, 0.0f, UniversalRenderPipeline::maxShadowBias); }

    void UniversalRenderPipelineAsset::SetShadowNormalBias(f32 value) { shadowNormalBias_ = mathf::Clamp(value, 0.0f, UniversalRenderPipeline::maxShadowBias); }

    void UniversalRenderPipelineAsset::SetColorGradingLutSize(i32 value) { colorGradingLutSize_ = mathf::Clamp(value, k_MinLutSize, k_MaxLutSize); }

    engine::Shader* UniversalRenderPipelineAsset::defaultShader() const { return engine::Shader::Find("Universal Render Pipeline/Lit"); }

    engine::Material* UniversalRenderPipelineAsset::defaultMaterial()
    {
        if (!defaultMaterial_.Get())
            if (engine::Shader* shader = defaultShader())
            {
                engine::Material* m = engine::Material::Create(shader);
                m->SetName("Lit");
                defaultMaterial_ = engine::PPtr<engine::Material>(m);
            }
        return defaultMaterial_.Get();
    }

    engine::Material* UniversalRenderPipelineAsset::defaultParticleMaterial()
    {
        if (!defaultParticleMaterial_.Get())
            if (engine::Shader* shader = engine::Shader::Find("Universal Render Pipeline/Particles/Unlit"))
            {
                engine::Material* m = engine::Material::Create(shader);
                m->SetName("ParticlesUnlit");
                defaultParticleMaterial_ = engine::PPtr<engine::Material>(m);
            }
        return defaultParticleMaterial_.Get();
    }

    engine::RenderPipeline* UniversalRenderPipelineAsset::CreatePipeline() { return new UniversalRenderPipeline(this); }

    // ---------------------------------------------------------------------------------------------- the pipeline

    i32 UniversalRenderPipeline::maxVisibleAdditionalLights()
    {
        rhi::Device* device = engine::Graphics::Device();
        rhi::Backend b = device ? device->Caps().backend : rhi::Backend::D3D11;
        return b == rhi::Backend::OpenGLES || b == rhi::Backend::WebGL2 ? 32 : 256;
    }

    UniversalRenderPipelineAsset* UniversalRenderPipeline::asset()
    {
        engine::ScriptableObject* current = engine::GraphicsSettings::currentRenderPipeline();
        if (!current || !scripting::IsAssignableFrom(scripting::TypeOf<UniversalRenderPipelineAsset>(), current->Class().type))
            return nullptr;
        return static_cast<UniversalRenderPipelineAsset*>(current);
    }

    UniversalRenderPipeline::UniversalRenderPipeline(UniversalRenderPipelineAsset* asset) : asset_(asset), graph_("Standard")
    {
        Blitter::Initialize(engine::Shader::Find("Hidden/Core/Blit"), engine::Shader::Find("Hidden/Core/BlitColorAndDepth"));
        VolumeManager& volumes = VolumeManager::instance();
        if (!volumes.isInitialized())
            volumes.Initialize(UniversalRenderPipelineAsset::globalVolumeProfile(), asset ? asset->volumeProfile() : nullptr);
        engine::GraphicsSettings::SetUseScriptableRenderPipelineBatching(asset && asset->useSRPBatcher());
    }

    UniversalRenderPipeline::~UniversalRenderPipeline() = default;

    void UniversalRenderPipeline::Dispose(bool disposing)
    {
        (void)disposing;
        post_.Dispose();
        graph_.Cleanup();
        frame_.Dispose();
        // The volume framework goes with the pipeline (the next one starts it with its asset's profiles).
        VolumeManager& volumes = VolumeManager::instance();
        if (volumes.isInitialized())
            volumes.Deinitialize();
    }

    void UniversalRenderPipeline::GetLightAttenuationAndSpotDirection(engine::LightType lightType, f32 lightRange, const Matrix4x4& lightLocalToWorldMatrix,
                                                                      f32 spotAngle, const f32* innerSpotAngle, Vector4& lightAttenuation, Vector4& lightSpotDir)
    {
        // Directional lights: no range (x 0, y 1); no spot (z 0, w 1).
        lightAttenuation = Vector4(0, 1, 0, 1);
        lightSpotDir = Vector4(0, 0, 1, 0);
        if (lightType == engine::LightType::Directional)
            return;
        // A smooth fade to zero at the range from 80% of it.
        f32 rangeSqr = lightRange * lightRange;
        f32 fadeStartSqr = 0.8f * 0.8f * rangeSqr;
        f32 fadeRangeSqr = fadeStartSqr - rangeSqr;
        lightAttenuation.x = 1.0f / Max(0.0001f, rangeSqr);
        lightAttenuation.y = -rangeSqr / fadeRangeSqr;
        if (lightType != engine::LightType::Spot)
            return;
        Vector4 dir = lightLocalToWorldMatrix.GetColumn(2);
        lightSpotDir = Vector4(-dir.x, -dir.y, -dir.z, 0);
        f32 cosOuter = std::cos(mathf::Deg2Rad * spotAngle * 0.5f);
        f32 cosInner = innerSpotAngle ? std::cos(*innerSpotAngle * mathf::Deg2Rad * 0.5f)
                                      : std::cos(2.0f * std::atan(std::tan(spotAngle * 0.5f * mathf::Deg2Rad) * (64.0f - 18.0f) / 64.0f) * 0.5f);
        f32 invRange = 1.0f / Max(0.001f, cosInner - cosOuter);
        lightAttenuation.z = invRange;
        lightAttenuation.w = -cosOuter * invRange;
    }

    void UniversalRenderPipeline::InitializeLightConstants_Common(Span<const engine::VisibleLight> lights, i32 lightIndex, Vector4& lightPos, Vector4& lightColor,
                                                                  Vector4& lightAttenuation, Vector4& lightSpotDir, Vector4& lightOcclusionProbeChannel)
    {
        lightPos = Vector4(0, 0, 1, 0);
        lightColor = Vector4(0, 0, 0, 0);
        lightAttenuation = Vector4(0, 1, 0, 1);
        lightSpotDir = Vector4(0, 0, 1, 0);
        lightOcclusionProbeChannel = Vector4(0, 0, 0, -1);
        if (lightIndex < 0 || usize(lightIndex) >= lights.Size())
            return;
        const engine::VisibleLight& light = lights[u32(lightIndex)];
        const Matrix4x4& m = light.localToWorldMatrix;
        if (light.lightType == engine::LightType::Directional)
        {
            Vector4 dir = m.GetColumn(2);
            lightPos = Vector4(-dir.x, -dir.y, -dir.z, 0);
        }
        else
        {
            Vector3 p = m.GetPosition();
            lightPos = Vector4(p.x, p.y, p.z, 1);
        }
        lightColor = Vector4(light.finalColor.r, light.finalColor.g, light.finalColor.b, light.finalColor.a);
        engine::Light* l = light.light();
        f32 inner = l ? l->innerSpotAngle() : 0;
        GetLightAttenuationAndSpotDirection(light.lightType, light.range, m, light.spotAngle, l ? &inner : nullptr, lightAttenuation, lightSpotDir);
    }

    void UniversalRenderPipeline::Render(engine::ScriptableRenderContext& context, Span<engine::Camera* const> cameras)
    {
        BeginContextRendering(context, cameras);
        for (engine::Camera* camera : cameras)
            if (camera)
                RenderCamera(context, camera);
        graph_.EndFrame();
        EndContextRendering(context, cameras);
    }

    void UniversalRenderPipeline::FillCameraData(engine::Camera* camera, ScriptableRenderer* renderer)
    {
        UniversalRenderPipelineAsset* asset = asset_.Get();
        auto* extra = static_cast<UniversalAdditionalCameraData*>(camera->gameObject() ? camera->gameObject()->GetComponent(
                                                                                             scripting::TypeOf<UniversalAdditionalCameraData>())
                                                                                       : nullptr);
        UniversalCameraData& c = frame_.GetOrCreate<UniversalCameraData>();
        c.camera = c.baseCamera = camera;
        c.renderType = extra ? extra->renderType() : CameraRenderType::Base;
        c.cameraType = camera->cameraType();
        c.targetTexture = camera->targetTexture();
        f32 scale = asset ? asset->renderScale() : 1.0f;
        c.renderScale = std::fabs(1.0f - scale) < 0.05f ? 1.0f : scale;
        c.scaledWidth = Max(1, i32(std::lround(f32(camera->pixelWidth()) * c.renderScale)));
        c.scaledHeight = Max(1, i32(std::lround(f32(camera->pixelHeight()) * c.renderScale)));
        Rect r = camera->rect();
        c.isDefaultViewport = r.x == 0 && r.y == 0 && r.width == 1 && r.height == 1;
        c.isHdrEnabled = camera->allowHDR() && asset && asset->supportsHDR();
        c.requiresDepthTexture = extra ? extra->requiresDepthTexture() : asset && asset->supportsCameraDepthTexture();
        c.requiresOpaqueTexture = extra ? extra->requiresColorTexture() : asset && asset->supportsCameraOpaqueTexture();
        c.postProcessEnabled = extra && extra->renderPostProcessing();
        bool shadows = !extra || extra->renderShadows();
        c.maxShadowDistance = shadows && asset ? Min(asset->shadowDistance(), camera->farClipPlane()) : 0.0f;
        c.backgroundColor = camera->backgroundColor();
        c.worldSpaceCameraPos = camera->transform() ? camera->transform()->position() : Vector3();
        c.volumeTrigger = extra && extra->volumeTrigger() ? extra->volumeTrigger() : camera->transform();
        c.volumeLayerMask = extra ? extra->volumeLayerMaskBits() : 1;
        c.renderer = renderer;
        c.rendersIntoTexture = true;
        // An HDR camera's color in the asset's precision.
        engine::RenderTextureDescriptor d(c.scaledWidth, c.scaledHeight, c.isHdrEnabled ? HdrColorFormat(asset) : GraphicsFormat::R8G8B8A8_UNorm, 32);
        d.msaaSamples = asset ? asset->msaaSampleCount() : 1;
        c.cameraTargetDescriptor = d;

        UniversalPostProcessingData& p = frame_.GetOrCreate<UniversalPostProcessingData>();
        p.isEnabled = c.postProcessEnabled;
        p.gradingMode = asset ? asset->colorGradingMode() : ColorGradingMode::LowDynamicRange;
        p.lutSize = asset ? asset->colorGradingLutSize() : 32;
        p.useFastSRGBLinearConversion = asset && asset->useFastSRGBLinearConversion();
    }

    void UniversalRenderPipeline::SetupLights(engine::CommandBuffer& cmd, engine::CullingResults culling)
    {
        UniversalRenderPipelineAsset* asset = asset_.Get();
        Span<const engine::VisibleLight> lights = culling.visibleLights();
        UniversalLightData& l = frame_.GetOrCreate<UniversalLightData>();
        l.visibleLights = lights;
        // The main light: the brightest directional light.
        l.mainLightIndex = -1;
        f32 brightest = -1;
        if (!asset || asset->mainLightRenderingMode() != LightRenderingMode::Disabled)
            for (u32 i = 0; i < lights.Size(); i++)
                if (lights[i].lightType == engine::LightType::Directional)
                {
                    f32 b = lights[i].finalColor.grayscale();
                    if (b > brightest)
                        brightest = b, l.mainLightIndex = i32(i);
                }
        // The additional lights: the others (up to the device's limit), in the per object lists by these indices.
        bool additional = !asset || asset->additionalLightsRenderingMode() != LightRenderingMode::Disabled;
        i32 limit = maxVisibleAdditionalLights();
        Array<i32> map;
        map.Resize(lights.Size());
        Array<Vector4> positions, colors, attenuations, spots;
        for (u32 i = 0; i < lights.Size(); i++)
        {
            map[i] = -1;
            if (i32(i) == l.mainLightIndex || !additional || i32(positions.Size()) >= limit)
                continue;
            map[i] = i32(positions.Size());
            Vector4 pos, color, attenuation, spot, probe;
            InitializeLightConstants_Common(lights, i32(i), pos, color, attenuation, spot, probe);
            positions.Push(pos), colors.Push(color), attenuations.Push(attenuation), spots.Push(spot);
        }
        culling.SetLightIndexMap(Span<const i32>(map.Data(), map.Size()));
        l.additionalLightsCount = i32(positions.Size());
        l.supportsAdditionalLights = additional;
        l.shadeAdditionalLightsPerVertex = asset && asset->additionalLightsRenderingMode() == LightRenderingMode::PerVertex;
        l.maxPerObjectAdditionalLightsCount = additional ? Min(asset ? asset->maxAdditionalLightsCount() : 4, maxPerObjectLights) : 0;

        Vector4 mainPos, mainColor, mainAttenuation, mainSpot, mainProbe;
        InitializeLightConstants_Common(lights, l.mainLightIndex, mainPos, mainColor, mainAttenuation, mainSpot, mainProbe);
        cmd.SetGlobalVector(engine::Shader::PropertyToID("_MainLightPosition"), mainPos);
        cmd.SetGlobalVector(engine::Shader::PropertyToID("_MainLightColor"), mainColor);
        cmd.SetGlobalVector(engine::Shader::PropertyToID("_AdditionalLightsCount"), Vector4(f32(l.maxPerObjectAdditionalLightsCount), 0, 0, 0));
        // The reflection without environment reflections: the ambient probe's first coefficients at the reflection
        // intensity (as the reference).
        engine::SphericalHarmonicsL2 ambient = engine::RenderSettings::ambientProbe();
        f32 intensity = engine::RenderSettings::reflectionIntensity();
        cmd.SetGlobalVector(engine::Shader::PropertyToID("_GlossyEnvironmentColor"),
                            Vector4(ambient(0, 0) * intensity, ambient(1, 0) * intensity, ambient(2, 0) * intensity, 1));
        // The arrays have the device's size (shaders declare that many).
        while (i32(positions.Size()) < limit)
            positions.Push(Vector4()), colors.Push(Vector4()), attenuations.Push(Vector4(0, 1, 0, 1)), spots.Push(Vector4(0, 0, 1, 0));
        cmd.SetGlobalVectorArray(engine::Shader::PropertyToID("_AdditionalLightsPosition"), Span<const Vector4>(positions.Data(), positions.Size()));
        cmd.SetGlobalVectorArray(engine::Shader::PropertyToID("_AdditionalLightsColor"), Span<const Vector4>(colors.Data(), colors.Size()));
        cmd.SetGlobalVectorArray(engine::Shader::PropertyToID("_AdditionalLightsAttenuation"), Span<const Vector4>(attenuations.Data(), attenuations.Size()));
        cmd.SetGlobalVectorArray(engine::Shader::PropertyToID("_AdditionalLightsSpotDir"), Span<const Vector4>(spots.Data(), spots.Size()));
    }

    void UniversalRenderPipeline::RenderCamera(engine::ScriptableRenderContext& context, engine::Camera* camera)
    {
        BeginCameraRendering(context, camera);
        UniversalRenderPipelineAsset* asset = asset_.Get();
        auto* extra = static_cast<UniversalAdditionalCameraData*>(camera->gameObject() ? camera->gameObject()->GetComponent(
                                                                                             scripting::TypeOf<UniversalAdditionalCameraData>())
                                                                                       : nullptr);
        ScriptableRenderer* renderer = asset ? asset->GetRenderer(extra ? extra->rendererIndex() : -1) : nullptr;
        engine::ScriptableCullingParameters parameters;
        if (renderer && !renderer->SupportsDevice())
        {
            HX_LOG_ERROR(gStandardLog, "Camera '{}': its renderer does not draw on this graphics device.", camera->Name());
            renderer = nullptr;
        }
        if (!renderer || !camera->TryGetCullingParameters(parameters))
        {
            EndCameraRendering(context, camera);
            return;
        }
        FillCameraData(camera, renderer);
        UniversalCameraData& cameraData = *frame_.Get<UniversalCameraData>();
        CameraData compatCamera{&frame_};
        renderer->CallOnCameraPreCull(compatCamera);

        // Culling: the asset's shadow distance and cascade spheres, the renderer's changes.
        parameters.shadowDistance = cameraData.maxShadowDistance;
        parameters.conservativeEnclosingSphere = asset->conservativeEnclosingSphere();
        parameters.numIterationsEnclosingSphere = asset->numIterationsEnclosingSphere();
        renderer->CallSetupCullingParameters(parameters, frame_);
        engine::CullingResults culling = context.Cull(parameters);
        context.SetupCameraProperties(camera);

        engine::CommandBuffer cmd;
        cmd.SetName("Standard");
        SetupLights(cmd, culling);
        UniversalLightData& lightData = *frame_.Get<UniversalLightData>();
        ScriptableRendererData* rendererData = renderer->data();
        UniversalRendererData* data = rendererData && scripting::IsAssignableFrom(scripting::TypeOf<UniversalRendererData>(), rendererData->Class().type)
                                          ? static_cast<UniversalRendererData*>(rendererData)
                                          : nullptr;

        UniversalRenderingData& rendering = frame_.GetOrCreate<UniversalRenderingData>();
        rendering.cullResults = culling;
        rendering.renderingMode = data ? data->renderingMode() : RenderingMode::Forward;
        rendering.supportsDynamicBatching = asset->supportsDynamicBatching();
        rendering.opaqueLayerMask = data ? data->opaqueLayerMaskBits() : -1;
        rendering.transparentLayerMask = data ? data->transparentLayerMaskBits() : -1;
        i32 flags = i32(engine::PerObjectData::ReflectionProbes) | i32(engine::PerObjectData::Lightmaps) | i32(engine::PerObjectData::LightProbe) |
                    i32(engine::PerObjectData::OcclusionProbe) | i32(engine::PerObjectData::ShadowMask);
        if (lightData.maxPerObjectAdditionalLightsCount > 0)
            flags |= i32(engine::PerObjectData::LightData) | i32(engine::PerObjectData::LightIndices);
        rendering.perObjectData = engine::PerObjectData(flags);

        UniversalShadowData& shadows = frame_.GetOrCreate<UniversalShadowData>();
        engine::Light* mainLight = lightData.mainLightIndex >= 0 ? lightData.visibleLights[u32(lightData.mainLightIndex)].light() : nullptr;
        Bounds casters;
        shadows.supportsMainLightShadows = asset->supportsMainLightShadows() && mainLight && mainLight->shadows() != engine::LightShadows::None &&
                                           cameraData.maxShadowDistance > 0 && culling.GetShadowCasterBounds(lightData.mainLightIndex, casters);
        shadows.supportsSoftShadows = asset->supportsSoftShadows() && mainLight && mainLight->shadows() == engine::LightShadows::Soft;
        i32 resolution = asset->mainLightShadowmapResolution();
        shadows.mainLightShadowCascadesCount = asset->shadowCascadeCount();
        shadows.mainLightShadowmapWidth = resolution;
        shadows.mainLightShadowmapHeight = shadows.mainLightShadowCascadesCount == 2 ? resolution / 2 : resolution;
        i32 cascades = shadows.mainLightShadowCascadesCount;
        shadows.mainLightShadowCascadesSplit = cascades == 2   ? Vector3(asset->cascade2Split(), 1, 0)
                                               : cascades == 3 ? Vector3(asset->cascade3Split().x, asset->cascade3Split().y, 0)
                                               : cascades == 4 ? asset->cascade4Split()
                                                               : Vector3(1, 0, 0);
        shadows.mainLightShadowCascadeBorder = asset->cascadeBorder();
        shadows.shadowmapDepthBufferBits = 16;
        shadows.bias.Clear();
        for (const engine::VisibleLight& v : lightData.visibleLights)
        {
            engine::Light* l = v.light();
            auto* lightExtra = l && l->gameObject() ? static_cast<UniversalAdditionalLightData*>(l->gameObject()->GetComponent(
                                                          scripting::TypeOf<UniversalAdditionalLightData>()))
                                                    : nullptr;
            bool pipeline = !lightExtra || lightExtra->usePipelineSettings();
            shadows.bias.Push(pipeline || !l ? Vector4(asset->shadowDepthBias(), asset->shadowNormalBias(), 0, 0) : Vector4(l->shadowBias(), l->shadowNormalBias(), 0, 0));
        }

        // The camera's volumes.
        if (cameraData.postProcessEnabled && (!extra || extra->requiresVolumeFrameworkUpdate()))
            VolumeManager::instance().Update(cameraData.volumeTrigger, engine::LayerMask(cameraData.volumeLayerMask));
        RTHandles::SetReferenceSize(cameraData.scaledWidth, cameraData.scaledHeight);

        // The features' passes, then the frame.
        RenderingData renderingData{&frame_};
        renderer->AddFeaturePasses(renderingData);
        renderer->RenderFrame(context, graph_, post_, frame_, cmd);
        context.ExecuteCommandBuffer(cmd);
        context.Submit();
        engine::CommandBuffer cleanup;
        renderer->FinishFrame(cleanup);
        frame_.Dispose();
        EndCameraRendering(context, camera);
    }
}
