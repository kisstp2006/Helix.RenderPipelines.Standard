#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipeline.h>

#include <Helix/Engine/Rendering/GL.h>
#include <Helix/Engine/Rendering/Light.h>
#include <Helix/Engine/Rendering/RenderSettings.h>
#include <Helix/RenderPipelines/Core/Utilities/Blitter.h>
#include <Helix/RenderPipelines/Core/Volume/Volume.h>
#include <Helix/RenderPipelines/Standard/Shadows/ShadowUtils.h>

#include <cmath>

// The renderers' frames on the core's render graph: the base frame of ScriptableRenderer (what C# renderers draw) and
// the forward renderer's, the passes in the order of their events.
namespace helix::rendering::universal
{
    using namespace engine;

    namespace
    {
        // The pass names the renderer draws (the Standard shaders' passes and the unlit passes of any shader).
        Array<ShaderTagId> ForwardPasses()
        {
            Array<ShaderTagId> names;
            names.Push(ShaderTagId("UniversalForward"));
            names.Push(ShaderTagId("UniversalForwardOnly"));
            names.Push(ShaderTagId("SRPDefaultUnlit"));
            return names;
        }

        struct DrawData
        {
            RendererListHandle list;
            RenderGraph* graph = nullptr;
        };

        struct BlitData
        {
            TextureHandle source;
            RenderGraph* graph = nullptr;
            bool bilinear = false;
            // A material's pass instead of the Blitter's own (null: the Blitter's).
            Material* material = nullptr;
            i32 pass = 0;
        };

        // A texture's tile in the atlas as a matrix from clip space (-1 to 1, depth 0 to 1 already) to the tile's texels
        // (0 to 1 of the atlas).
        // What the queued passes ask of the camera: its depth or color as textures, an intermediate texture.
        struct PassNeeds
        {
            bool depth = false, color = false, intermediate = false;
        };

        PassNeeds NeedsOf(Span<ScriptableRenderPass* const> passes)
        {
            PassNeeds n;
            for (const ScriptableRenderPass* p : passes)
            {
                n.depth = n.depth || (i32(p->input()) & i32(ScriptableRenderPassInput::Depth)) != 0;
                n.color = n.color || (i32(p->input()) & i32(ScriptableRenderPassInput::Color)) != 0;
                n.intermediate = n.intermediate || p->requiresIntermediateTexture();
            }
            return n;
        }

        // Whether the camera is drawn into textures of its own and then blitted into its target: always for the
        // Always mode; for Auto when the render scale, the viewport, post processing, HDR or MSAA need a texture the
        // target is not, a depth or opaque texture is copied, a pass needs it, or the target texture has no depth.
        bool NeedsIntermediate(const UniversalCameraData& c, const UniversalPostProcessingData& post, IntermediateTextureMode mode, const PassNeeds& needs)
        {
            if (mode == IntermediateTextureMode::Always)
                return true;
            if (c.renderScale != 1.0f || !c.isDefaultViewport || post.isEnabled || c.isHdrEnabled || c.cameraTargetDescriptor.msaaSamples > 1)
                return true;
            if (c.requiresDepthTexture || c.requiresOpaqueTexture || needs.intermediate)
                return true;
            return c.targetTexture && c.targetTexture->depth() == 0;
        }

        // The camera's color and depth: textures of their own (intermediate) or the camera's target, cleared by the
        // camera's flags.
        void SetupCameraTargets(RenderGraph& graph, UniversalCameraData& c, UniversalResourceData& r, bool intermediate)
        {
            Camera* camera = c.camera;
            CameraClearFlags clear = camera->clearFlags();
            bool clearColor = clear == CameraClearFlags::SolidColor || clear == CameraClearFlags::Skybox;
            Color background = clear == CameraClearFlags::Skybox ? Color(0, 0, 0, 1) : camera->backgroundColor();
            RenderTargetInfo info;
            info.width = camera->pixelWidth();
            info.height = camera->pixelHeight();
            ImportResourceParams colorImport, depthImport;
            colorImport.clearOnFirstUse = !intermediate && clearColor;
            colorImport.clearColor = background;
            depthImport.clearOnFirstUse = !intermediate && clear != CameraClearFlags::Nothing;
            r.backBufferColor = graph.ImportBackbuffer(RenderTargetIdentifier(BuiltinRenderTextureType::CameraTarget), info, colorImport);
            r.backBufferDepth = graph.ImportBackbuffer(RenderTargetIdentifier(BuiltinRenderTextureType::CameraTarget), info, depthImport);
            c.rendersIntoTexture = intermediate || c.targetTexture != nullptr;
            if (!intermediate)
            {
                r.cameraColor = r.backBufferColor;
                r.cameraDepth = r.backBufferDepth;
                r.isActiveTargetBackBuffer = true;
                return;
            }
            TextureDesc colorDesc(c.scaledWidth, c.scaledHeight);
            colorDesc.colorFormat = c.cameraTargetDescriptor.graphicsFormat;
            colorDesc.clearBuffer = clearColor;
            colorDesc.clearColor = background;
            colorDesc.filterMode = FilterMode::Bilinear;
            colorDesc.name = String("_CameraColorAttachment");
            // The camera target's samples (the engine resolves the color when it stops being the target).
            colorDesc.msaaSamples = MSAASamples(Max(1, c.cameraTargetDescriptor.msaaSamples));
            r.cameraColor = graph.CreateTexture(colorDesc);
            TextureDesc depthDesc(c.scaledWidth, c.scaledHeight);
            depthDesc.colorFormat = GraphicsFormat::None;
            depthDesc.depthBufferBits = DepthBits::Depth32;
            depthDesc.clearBuffer = clear != CameraClearFlags::Nothing;
            depthDesc.name = String("_CameraDepthAttachment");
            depthDesc.msaaSamples = colorDesc.msaaSamples;
            r.cameraDepth = graph.CreateTexture(depthDesc);
            r.isActiveTargetBackBuffer = false;
        }

        // The camera's color into its target (scaled when the render scale is not 1); nothing to do when it was drawn
        // there.
        void FinalBlit(RenderGraph& graph, const UniversalCameraData& c, UniversalResourceData& r)
        {
            if (r.isActiveTargetBackBuffer)
                return;
            BlitData* data = nullptr;
            PassBuilder<BlitData> b = graph.AddRasterRenderPass("FinalBlit", data);
            data->source = r.cameraColor;
            data->graph = &graph;
            UniversalRenderPipelineAsset* asset = UniversalRenderPipeline::asset();
            data->bilinear = c.renderScale != 1.0f && !(asset && asset->upscalingFilter() == UpscalingFilterSelection::Point);
            b.UseTexture(r.cameraColor);
            b.SetRenderAttachment(r.backBufferColor, 0, AccessFlags::WriteAll);
            b.SetRenderFunc([](BlitData& d, RasterGraphContext& ctx) {
                if (RTHandle* source = d.graph->Resolve(d.source))
                    Blitter::BlitTexture(*ctx.cmd, *source, Vector4(1, 1, 0, 0), 0, d.bilinear);
            });
            b.Dispose();
            r.isActiveTargetBackBuffer = true;
        }

        RenderGraphParameters ParametersOf(ScriptableRenderContext& context, CommandBuffer& cmd, Camera* camera)
        {
            RenderGraphParameters parameters;
            parameters.commandBuffer = &cmd;
            parameters.scriptableRenderContext = &context;
            parameters.executionName = String(camera->Name());
            return parameters;
        }

        constexpr RenderPassEvent After(RenderPassEvent e) { return RenderPassEvent(i32(e) + 1); }
    }

    // ---------------------------------------------------------------------------------------------- the base frame

    void ScriptableRenderer::RenderFrame(ScriptableRenderContext& context, RenderGraph& graph, PostProcessPasses& post, ContextContainer& frame, CommandBuffer& cmd)
    {
        (void)post;
        UniversalCameraData& cameraData = *frame.Get<UniversalCameraData>();
        UniversalPostProcessingData& postData = *frame.Get<UniversalPostProcessingData>();
        UniversalResourceData& resources = frame.GetOrCreate<UniversalResourceData>();
        graph.nativeRenderPassesEnabled = data() && data()->useNativeRenderPass();
        if (!graph.BeginRecording(ParametersOf(context, cmd, cameraData.camera)))
            return;
        OnBeginRenderGraphFrame();
        SortPasses();
        SetupCameraTargets(graph, cameraData, resources, NeedsIntermediate(cameraData, postData, IntermediateTextureMode::Auto, NeedsOf(passes())));
        RecordPasses(graph, frame, RenderPassEvent::BeforeRendering, RenderPassEvent::AfterRendering);
        FinalBlit(graph, cameraData, resources);
        RecordPasses(graph, frame, RenderPassEvent::AfterRendering, RenderPassEvent(i32(RenderPassEvent::AfterRendering) + 0x40000000));
        OnEndRenderGraphFrame();
        graph.EndRecordingAndExecute();
    }

    // ---------------------------------------------------------------------------------------------- the forward frame

    UniversalRenderer::~UniversalRenderer()
    {
        for (Material* m : {copyDepth_, sampling_})
            if (m)
                Object::DestroyImmediate(m);
    }

    void UniversalRenderer::EnsureMaterials()
    {
        if (!copyDepth_)
            if (Shader* s = Shader::Find("Hidden/Universal Render Pipeline/CopyDepth"))
                copyDepth_ = Material::Create(s);
        if (!sampling_)
            if (Shader* s = Shader::Find("Hidden/Universal Render Pipeline/Sampling"))
                sampling_ = Material::Create(s);
    }

    void UniversalRenderer::RenderFrame(ScriptableRenderContext& context, RenderGraph& graph, PostProcessPasses& post, ContextContainer& frame, CommandBuffer& cmd)
    {
        UniversalCameraData& cameraData = *frame.Get<UniversalCameraData>();
        UniversalRenderingData& renderingData = *frame.Get<UniversalRenderingData>();
        UniversalLightData& lightData = *frame.Get<UniversalLightData>();
        UniversalShadowData& shadowData = *frame.Get<UniversalShadowData>();
        UniversalPostProcessingData& postData = *frame.Get<UniversalPostProcessingData>();
        UniversalResourceData& resources = frame.GetOrCreate<UniversalResourceData>();
        Camera* camera = cameraData.camera;
        UniversalRendererData* settings = rendererData();
        UniversalRenderPipelineAsset* asset = UniversalRenderPipeline::asset();

        graph.nativeRenderPassesEnabled = settings && settings->useNativeRenderPass();
        if (!graph.BeginRecording(ParametersOf(context, cmd, camera)))
            return;
        OnBeginRenderGraphFrame();
        SortPasses();
        EnsureMaterials();

        // What the passes ask for: the depth and opaque textures, the intermediate texture.
        PassNeeds needs = NeedsOf(passes());
        cameraData.requiresDepthTexture = cameraData.requiresDepthTexture || needs.depth;
        cameraData.requiresOpaqueTexture = cameraData.requiresOpaqueTexture || needs.color;
        CopyDepthMode copyMode = settings ? settings->copyDepthMode() : CopyDepthMode::AfterOpaques;
        // A multisampled depth is not read as a texture: with MSAA the depth texture comes from the prepass.
        bool prepass = cameraData.requiresDepthTexture && (copyMode == CopyDepthMode::ForcePrepass || cameraData.cameraTargetDescriptor.msaaSamples > 1);
        bool intermediate = NeedsIntermediate(cameraData, postData, settings ? settings->intermediateTextureMode() : IntermediateTextureMode::Auto, needs);
        SetupCameraTargets(graph, cameraData, resources, intermediate);

        auto record = [&](RenderPassEvent from, RenderPassEvent to) { RecordPasses(graph, frame, from, to); };
        auto listDesc = [&](bool opaque, Array<ShaderTagId> names) {
            RendererUtils::RendererListDesc desc;
            desc.passNames = Move(names);
            desc.cullingResults = renderingData.cullResults;
            desc.camera = camera;
            desc.sortingCriteria = opaque ? cameraData.defaultOpaqueSortFlags : SortingCriteria::CommonTransparent;
            desc.renderQueueRange = opaque ? RenderQueueRange::opaque() : RenderQueueRange::transparent();
            desc.layerMask = opaque ? renderingData.opaqueLayerMask : renderingData.transparentLayerMask;
            desc.rendererConfiguration = renderingData.perObjectData;
            if (settings && settings->defaultStencilState().overrideStencilState)
            {
                const StencilStateData& s = settings->defaultStencilState();
                desc.hasStateBlock = true;
                desc.stateBlock.mask = RenderStateMask::Stencil;
                desc.stateBlock.stencilReference = s.stencilReference;
                desc.stateBlock.stencilState.enabled = true;
                desc.stateBlock.stencilState.compareFunctionFront = desc.stateBlock.stencilState.compareFunctionBack = s.stencilCompareFunction;
                desc.stateBlock.stencilState.passOperationFront = desc.stateBlock.stencilState.passOperationBack = s.passOperation;
                desc.stateBlock.stencilState.failOperationFront = desc.stateBlock.stencilState.failOperationBack = s.failOperation;
                desc.stateBlock.stencilState.zFailOperationFront = desc.stateBlock.stencilState.zFailOperationBack = s.zFailOperation;
            }
            return desc;
        };
        auto draw = [&](StringView name, RendererListHandle list, TextureHandle color, TextureHandle depth) {
            DrawData* data = nullptr;
            PassBuilder<DrawData> b = graph.AddRasterRenderPass(name, data);
            data->list = list;
            data->graph = &graph;
            b.UseRendererList(list);
            if (color.IsValid())
                b.SetRenderAttachment(color, 0);
            b.SetRenderAttachmentDepth(depth);
            b.AllowGlobalStateModification(true);
            // The objects' shaders read the frame's global textures (the shadow atlas, the depth and opaque copies).
            b.UseAllGlobalTextures(true);
            b.SetRenderFunc([](DrawData& d, RasterGraphContext& ctx) { ctx.cmd->DrawRendererList(d.graph->ResolveList(d.list)); });
        };
        auto drawList = [&](StringView name, bool opaque) {
            draw(name, graph.CreateRendererList(listDesc(opaque, ForwardPasses())), resources.cameraColor, resources.cameraDepth);
        };
        // The camera's depth into _CameraDepthTexture (a depth texture of its own).
        auto depthTexture = [&]() {
            TextureDesc desc(cameraData.scaledWidth, cameraData.scaledHeight);
            desc.colorFormat = GraphicsFormat::None;
            desc.depthBufferBits = DepthBits::Depth32;
            desc.filterMode = FilterMode::Point;
            desc.clearBuffer = true;
            desc.name = String("_CameraDepthTexture");
            return graph.CreateTexture(desc);
        };
        auto copyDepth = [&]() {
            if (!copyDepth_)
                return;
            resources.cameraDepthTexture = depthTexture();
            BlitData* data = nullptr;
            PassBuilder<BlitData> b = graph.AddRasterRenderPass("CopyDepth", data);
            data->source = resources.cameraDepth;
            data->graph = &graph;
            data->material = copyDepth_;
            b.UseTexture(resources.cameraDepth);
            b.SetRenderAttachmentDepth(resources.cameraDepthTexture, AccessFlags::WriteAll);
            b.SetGlobalTextureAfterPass(resources.cameraDepthTexture, Shader::PropertyToID("_CameraDepthTexture"));
            b.SetRenderFunc([](BlitData& d, RasterGraphContext& ctx) {
                if (RTHandle* source = d.graph->Resolve(d.source))
                    Blitter::BlitTexture(*ctx.cmd, *source, Vector4(1, 1, 0, 0), d.material, d.pass);
            });
        };
        // The camera's color after the skybox into _CameraOpaqueTexture, downsampled as the asset says.
        auto copyColor = [&]() {
            Downsampling down = asset ? asset->opaqueDownsampling() : Downsampling::None;
            i32 divisor = down == Downsampling::_2xBilinear ? 2 : down == Downsampling::_4xBox || down == Downsampling::_4xBilinear ? 4 : 1;
            TextureDesc desc(Max(1, cameraData.scaledWidth / divisor), Max(1, cameraData.scaledHeight / divisor));
            desc.colorFormat = cameraData.cameraTargetDescriptor.graphicsFormat;
            desc.filterMode = FilterMode::Bilinear;
            desc.name = String("_CameraOpaqueTexture");
            resources.cameraOpaqueTexture = graph.CreateTexture(desc);
            BlitData* data = nullptr;
            PassBuilder<BlitData> b = graph.AddRasterRenderPass("CopyColor", data);
            data->source = resources.cameraColor;
            data->graph = &graph;
            data->bilinear = down != Downsampling::None;
            if (down == Downsampling::_4xBox && sampling_)
            {
                data->material = sampling_;
                sampling_->SetFloat("_SampleOffset", 2.0f);
            }
            b.UseTexture(resources.cameraColor);
            b.SetRenderAttachment(resources.cameraOpaqueTexture, 0, AccessFlags::WriteAll);
            b.SetGlobalTextureAfterPass(resources.cameraOpaqueTexture, Shader::PropertyToID("_CameraOpaqueTexture"));
            b.SetRenderFunc([](BlitData& d, RasterGraphContext& ctx) {
                RTHandle* source = d.graph->Resolve(d.source);
                if (!source)
                    return;
                if (d.material)
                    Blitter::BlitTexture(*ctx.cmd, *source, Vector4(1, 1, 0, 0), d.material, d.pass);
                else
                    Blitter::BlitTexture(*ctx.cmd, *source, Vector4(1, 1, 0, 0), 0, d.bilinear);
            });
        };

        // Before the shadows; the main light's shadows; after them.
        record(RenderPassEvent::BeforeRendering, RenderPassEvent::BeforeRenderingShadows);
        if (shadowData.supportsMainLightShadows && lightData.mainLightIndex >= 0)
            RecordMainLightShadows(context, graph, frame, cmd);
        record(RenderPassEvent::BeforeRenderingShadows, RenderPassEvent::BeforeRenderingPrePasses);

        // The depth prepass (the DepthOnly passes into _CameraDepthTexture) when the renderer data forces it.
        if (prepass)
        {
            Array<ShaderTagId> names;
            names.Push(ShaderTagId("DepthOnly"));
            resources.cameraDepthTexture = depthTexture();
            DrawData* data = nullptr;
            PassBuilder<DrawData> b = graph.AddRasterRenderPass("DepthPrepass", data);
            data->list = graph.CreateRendererList(listDesc(true, Move(names)));
            data->graph = &graph;
            b.UseRendererList(data->list);
            b.SetRenderAttachmentDepth(resources.cameraDepthTexture, AccessFlags::WriteAll);
            b.SetGlobalTextureAfterPass(resources.cameraDepthTexture, Shader::PropertyToID("_CameraDepthTexture"));
            b.SetRenderFunc([](DrawData& d, RasterGraphContext& ctx) { ctx.cmd->DrawRendererList(d.graph->ResolveList(d.list)); });
        }
        record(RenderPassEvent::BeforeRenderingPrePasses, After(RenderPassEvent::BeforeRenderingOpaques));

        // Opaque, the depth copy, the skybox, the color copy, transparent.
        drawList("DrawOpaqueObjects", true);
        record(After(RenderPassEvent::BeforeRenderingOpaques), RenderPassEvent::AfterRenderingOpaques);
        if (cameraData.requiresDepthTexture && !prepass && copyMode == CopyDepthMode::AfterOpaques)
            copyDepth();
        record(RenderPassEvent::AfterRenderingOpaques, After(RenderPassEvent::BeforeRenderingSkybox));
        if (camera->clearFlags() == CameraClearFlags::Skybox && RenderSettings::skybox())
            draw("DrawSkybox", graph.CreateSkyboxRendererList(camera), resources.cameraColor, resources.cameraDepth);
        record(After(RenderPassEvent::BeforeRenderingSkybox), RenderPassEvent::AfterRenderingSkybox);
        if (cameraData.requiresOpaqueTexture)
            copyColor();
        record(RenderPassEvent::AfterRenderingSkybox, After(RenderPassEvent::BeforeRenderingTransparents));
        VolumeStack* stack = VolumeManager::instance().stack();
        if (postData.isEnabled)
            resources.cameraColor = post.Record(graph, stack, PostProcessInjectionPoint::AfterOpaqueAndSky, resources.cameraColor);
        drawList("DrawTransparentObjects", false);
        record(After(RenderPassEvent::BeforeRenderingTransparents), RenderPassEvent::AfterRenderingTransparents);
        if (cameraData.requiresDepthTexture && !prepass && copyMode == CopyDepthMode::AfterTransparents)
            copyDepth();
        record(RenderPassEvent::AfterRenderingTransparents, After(RenderPassEvent::BeforeRenderingPostProcessing));

        // Post processing: the core's effects before it, the color grading table, bloom and the uber pass, the core's
        // effects after it.
        if (postData.isEnabled)
        {
            resources.cameraColor = post.Record(graph, stack, PostProcessInjectionPoint::BeforePostProcess, resources.cameraColor);
            // The renderer data's shaders (no post processing pass without them).
            if (PostProcessData* postResources = settings ? settings->postProcessData() : nullptr)
            {
                TextureHandle lut = lutPass_.Record(graph, cameraData, postData, stack, *postResources);
                resources.cameraColor = postPass_.Record(graph, cameraData, postData, stack, *postResources, resources.cameraColor, lut);
            }
            resources.cameraColor = post.Record(graph, stack, PostProcessInjectionPoint::AfterPostProcess, resources.cameraColor);
            resources.afterPostProcessColor = resources.cameraColor;
        }
        record(After(RenderPassEvent::BeforeRenderingPostProcessing), RenderPassEvent::AfterRendering);

        // The camera's color into its target (when it was drawn into its own texture).
        FinalBlit(graph, cameraData, resources);
        record(RenderPassEvent::AfterRendering, RenderPassEvent(i32(RenderPassEvent::AfterRendering) + 0x40000000));
        OnEndRenderGraphFrame();
        graph.EndRecordingAndExecute();
    }

    void UniversalRenderer::RecordMainLightShadows(ScriptableRenderContext& context, RenderGraph& graph, ContextContainer& frame, CommandBuffer& cmd)
    {
        UniversalCameraData& cameraData = *frame.Get<UniversalCameraData>();
        UniversalRenderingData& renderingData = *frame.Get<UniversalRenderingData>();
        UniversalLightData& lightData = *frame.Get<UniversalLightData>();
        UniversalShadowData& shadowData = *frame.Get<UniversalShadowData>();
        UniversalResourceData& resources = frame.GetOrCreate<UniversalResourceData>();
        i32 lightIndex = lightData.mainLightIndex;
        const VisibleLight& main = lightData.visibleLights[u32(lightIndex)];
        Light* light = main.light();
        // The core's cascades with the pipeline's settings (the asset's or the light's bias, soft shadow tiers).
        CascadedShadowSettings s;
        s.lightIndex = lightIndex;
        s.cascadeCount = shadowData.mainLightShadowCascadesCount;
        s.splits = shadowData.mainLightShadowCascadesSplit;
        s.atlasWidth = shadowData.mainLightShadowmapWidth;
        s.atlasHeight = shadowData.mainLightShadowmapHeight;
        s.depthBits = shadowData.shadowmapDepthBufferBits;
        Vector4 bias = u32(lightIndex) < shadowData.bias.Size() ? shadowData.bias[u32(lightIndex)] : Vector4(1, 1, 0, 0);
        s.bias = Vector2(bias.x, bias.y);
        s.soft = shadowData.supportsSoftShadows && light && light->shadows() == LightShadows::Soft;
        s.softQuality = ShadowUtils::SoftShadowQualityToShaderProperty(light, s.soft);
        s.strength = light ? light->shadowStrength() : 1.0f;
        s.shadowDistance = cameraData.maxShadowDistance;
        s.cascadeBorder = shadowData.mainLightShadowCascadeBorder;
        s.nearPlane = light ? light->shadowNearPlane() : 0.2f;
        resources.mainShadowsTexture = CascadedShadows::Record(graph, context, renderingData.cullResults, s, cameraData.GetViewMatrix(),
                                                                cameraData.GetProjectionMatrix(), cmd);
    }
}
