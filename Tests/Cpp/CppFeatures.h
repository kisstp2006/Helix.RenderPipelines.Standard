#pragma once

#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Rendering/Light.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/RenderPipelines/Standard/Frame/FrameData.h>
#include <Helix/RenderPipelines/Standard/Renderer/ScriptableRenderer.h>
#include <Helix/RenderPipelines/Standard/Shadows/ShadowUtils.h>

#include <cmath>
#include <memory>

// A C++ feature of the tests: a pass of C++ queued at an event, noting what it sees of the frame.
namespace helix::rpstandard::testing
{
    inline Array<String>& CppNotes()
    {
        static Array<String> notes;
        return notes;
    }

    // Whether the C++ test pass asks for the intermediate texture.
    inline bool& CppPassRequiresIntermediate()
    {
        static bool value = false;
        return value;
    }

    class CppNotePass final : public rendering::universal::ScriptableRenderPass
    {
    public:
        void RecordRenderGraph(rendering::RenderGraph& graph, rendering::ContextContainer& frameData) override
        {
            (void)graph;
            using namespace rendering::universal;
            UniversalCameraData* camera = frameData.Get<UniversalCameraData>();
            UniversalResourceData* resources = frameData.Get<UniversalResourceData>();
            CppNotes().Push(Format("cpp:{}:{}:{}:{}", i32(renderPassEvent()), camera ? camera->scaledWidth : -1,
                                   resources && resources->cameraColor.IsValid() ? "color" : "none",
                                   resources && resources->isActiveTargetBackBuffer ? "back" : "texture"));
        }
    };

    HX_CLASS(Namespace = "StandardTests")
    class CppTestFeature : public rendering::universal::ScriptableRendererFeature
    {
    public:
        void Create() override
        {
            pass_ = std::make_shared<CppNotePass>();
            pass_->SetRenderPassEvent(rendering::universal::RenderPassEvent::AfterRenderingOpaques);
        }
        void AddRenderPasses(rendering::universal::ScriptableRenderer& renderer, rendering::universal::RenderingData& renderingData) override
        {
            (void)renderingData;
            if (!pass_)
                return;
            pass_->SetRequiresIntermediateTexture(CppPassRequiresIntermediate());
            renderer.EnqueuePass(pass_.get());
        }

    private:
        std::shared_ptr<CppNotePass> pass_;
    };

    // A value in thousandths (or ten thousandths), rounded half away from zero as C#'s note does.
    inline i64 Milli(f32 v, f32 scale = 1000.0f) { return i64(std::llround(f64(v) * f64(scale))); }

    // Notes the main light's cascades as ShadowUtils makes them (the same text as the C# pass's notes).
    class CppShadowPass final : public rendering::universal::ScriptableRenderPass
    {
    public:
        void RecordRenderGraph(rendering::RenderGraph& graph, rendering::ContextContainer& frameData) override
        {
            (void)graph;
            using namespace rendering::universal;
            UniversalRenderingData* rendering = frameData.Get<UniversalRenderingData>();
            UniversalLightData* lights = frameData.Get<UniversalLightData>();
            UniversalShadowData* shadows = frameData.Get<UniversalShadowData>();
            if (!rendering || !lights || !shadows || lights->mainLightIndex < 0 || !shadows->supportsMainLightShadows)
                return;
            i32 count = shadows->mainLightShadowCascadesCount;
            i32 width = shadows->mainLightShadowmapWidth, height = shadows->mainLightShadowmapHeight;
            i32 tile = ShadowUtils::GetMaxTileResolutionInAtlas(width, height, count);
            CppNotes().Push(Format("atlas:{}:{}:{}:{}", count, width, height, tile));
            const engine::VisibleLight& light = lights->visibleLights[u32(lights->mainLightIndex)];
            for (i32 i = 0; i < count; i++)
            {
                Vector4 sphere;
                ShadowSliceData slice;
                bool ok = ShadowUtils::ExtractDirectionalLightMatrix(rendering->cullResults, *shadows, lights->mainLightIndex, i, width, height, tile,
                                                                     light.light()->shadowNearPlane(), sphere, slice);
                Vector4 bias = ShadowUtils::GetShadowBias(light, lights->mainLightIndex, *shadows, slice.projectionMatrix, f32(tile));
                CppNotes().Push(Format("cascade:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}", i, ok ? 1 : 0, slice.offsetX, slice.offsetY, slice.resolution,
                                       Milli(sphere.x), Milli(sphere.y), Milli(sphere.z), Milli(sphere.w), Milli(slice.shadowTransform(0, 0), 10000.0f),
                                       Milli(slice.shadowTransform(0, 3), 10000.0f), Milli(slice.shadowTransform(1, 3), 10000.0f),
                                       Milli(bias.x, 100000.0f), Milli(bias.y, 100000.0f)));
            }
        }
    };

    HX_CLASS(Namespace = "StandardTests")
    class CppShadowFeature : public rendering::universal::ScriptableRendererFeature
    {
    public:
        void Create() override
        {
            pass_ = std::make_shared<CppShadowPass>();
            pass_->SetRenderPassEvent(rendering::universal::RenderPassEvent::BeforeRenderingShadows);
        }
        void AddRenderPasses(rendering::universal::ScriptableRenderer& renderer, rendering::universal::RenderingData& renderingData) override
        {
            (void)renderingData;
            if (pass_)
                renderer.EnqueuePass(pass_.get());
        }

    private:
        std::shared_ptr<CppShadowPass> pass_;
    };

    // Draws the opaque objects of layer 4 again with an override material (a flat color) after the opaques.
    class CppOverridePass final : public rendering::universal::ScriptableRenderPass
    {
    public:
        engine::Material* material = nullptr;

        void RecordRenderGraph(rendering::RenderGraph& graph, rendering::ContextContainer& frameData) override
        {
            using namespace rendering;
            using namespace rendering::universal;
            UniversalCameraData* camera = frameData.Get<UniversalCameraData>();
            UniversalRenderingData* rendering = frameData.Get<UniversalRenderingData>();
            UniversalResourceData* resources = frameData.Get<UniversalResourceData>();
            if (!camera || !rendering || !resources || !material)
                return;
            struct Data
            {
                RendererListHandle list;
                RenderGraph* graph = nullptr;
            };
            engine::RendererUtils::RendererListDesc desc;
            desc.passNames.Push(engine::ShaderTagId("UniversalForward"));
            desc.passNames.Push(engine::ShaderTagId("SRPDefaultUnlit"));
            desc.cullingResults = rendering->cullResults;
            desc.camera = camera->camera;
            desc.sortingCriteria = engine::SortingCriteria::CommonOpaque;
            desc.renderQueueRange = engine::RenderQueueRange::opaque();
            desc.layerMask = 1 << 4;
            desc.overrideMaterial = material;
            Data* data = nullptr;
            PassBuilder<Data> b = graph.AddRasterRenderPass("CppOverride", data);
            data->graph = &graph;
            data->list = graph.CreateRendererList(desc);
            b.UseRendererList(data->list);
            b.SetRenderAttachment(resources->activeColorTexture(), 0);
            b.SetRenderAttachmentDepth(resources->activeDepthTexture());
            b.SetRenderFunc([](Data& d, RasterGraphContext& ctx) { ctx.cmd->DrawRendererList(d.graph->ResolveList(d.list)); });
        }
    };

    HX_CLASS(Namespace = "StandardTests")
    class CppOverrideFeature : public rendering::universal::ScriptableRendererFeature
    {
    public:
        ~CppOverrideFeature() override
        {
            if (pass_ && pass_->material)
                engine::DestroyImmediate(pass_->material);
        }
        void Create() override
        {
            pass_ = std::make_shared<CppOverridePass>();
            pass_->SetRenderPassEvent(rendering::universal::RenderPassEvent::AfterRenderingOpaques);
            pass_->material = engine::Material::Create(engine::Shader::Find("Tests/StandardUnlit"));
            if (pass_->material)
                pass_->material->SetColor("_BaseColor", Color(1, 0.85f, 0.1f, 1));
        }
        void AddRenderPasses(rendering::universal::ScriptableRenderer& renderer, rendering::universal::RenderingData& renderingData) override
        {
            (void)renderingData;
            if (pass_)
                renderer.EnqueuePass(pass_.get());
        }

    private:
        std::shared_ptr<CppOverridePass> pass_;
    };

    // The test meshes for C# scripts (a box of side 1, a sphere of diameter 1).
    HX_CLASS(Namespace = "StandardTests", Static)
    class CppTestMeshes
    {
    public:
        HX_FUNCTION() static engine::Mesh* Box();
        HX_FUNCTION() static engine::Mesh* Sphere();
    };
}
