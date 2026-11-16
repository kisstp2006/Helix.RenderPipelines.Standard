#include <Helix/RenderPipelines/Standard/Renderer/ScriptableRenderer.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Engine/Rendering/GL.h>
#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Scripting/Bindings/Interop.h>

#include <algorithm>

namespace helix::rendering::universal
{
    namespace
    {
        void (*gManagedFeature)(u64 wrapper, i32 call, u64 renderer, u64 frame) = nullptr;
        u64 (*gManagedData)(u64 wrapper, u64* handle) = nullptr;
        u8 (*gManagedCulling)(u64 wrapper, u64 parameters, u64 frame) = nullptr;
        u8 (*gManagedDevice)(u64 wrapper, i32 device) = nullptr;

        bool IsCSharpScript(const engine::ScriptableObject& o)
        {
            const engine::ScriptClass* s = o.script();
            return s && s->language == engine::ScriptLanguage::CSharp;
        }

        struct Renderers
        {
            HashMap<u64, ScriptableRenderer*> all;
            u64 next = 0;
        };

        Renderers& R()
        {
            static Renderers r;
            return r;
        }
    }

    // ---------------------------------------------------------------------------------------------- data

    ScriptableRendererData::~ScriptableRendererData() = default;

    Array<ScriptableRendererFeature*> ScriptableRendererData::rendererFeatures() const
    {
        Array<ScriptableRendererFeature*> out;
        for (const engine::PPtr<ScriptableRendererFeature>& f : features_)
            out.Push(f.Get());
        return out;
    }

    void ScriptableRendererData::SetRendererFeatures(Array<ScriptableRendererFeature*> features)
    {
        features_.Clear();
        featureMap_.Clear();
        for (ScriptableRendererFeature* f : features)
        {
            features_.Push(engine::PPtr<ScriptableRendererFeature>(f));
            featureMap_.Push(f ? i64(engine::EntityId::ToULong(f->GetEntityId())) : 0);
        }
        SetDirty();
    }

    ScriptableRendererFeature* ScriptableRendererData::FindRendererFeature(scripting::TypeId type) const
    {
        for (const engine::PPtr<ScriptableRendererFeature>& p : features_)
            if (ScriptableRendererFeature* f = p.Get(); f && scripting::IsAssignableFrom(type, f->Class().type))
                return f;
        return nullptr;
    }

    void ScriptableRendererData::SetUseNativeRenderPass(bool value)
    {
        useNativeRenderPass_ = value;
        SetDirty();
    }

    bool ScriptableRendererData::IsCSharp() const { return IsCSharpScript(*this); }

    void ScriptableRendererData::OnEnable()
    {
        EnsureResources();
        if (!IsCSharp())
            SetDirty();
    }

    void UniversalRendererData::EnsureResources()
    {
        if (!postProcessData_.Get())
            postProcessData_ = engine::PPtr<PostProcessData>(PostProcessData::Default());
    }

    void ScriptableRendererData::OnValidate()
    {
        if (!IsCSharp())
            SetDirty();
    }

    ScriptableRenderer* ScriptableRendererData::CallCreateRenderer(u64& handle)
    {
        handle = 0;
        u64 wrapper = scripting::ObjectHandles<engine::Object>::WrapperOf(this);
        if (IsCSharp() && gManagedData && wrapper)
            return ScriptableRenderer::Find(gManagedData(wrapper, &handle));
        return CreateRenderer();
    }

    void ScriptableRendererData::SetManaged(u64 create) { gManagedData = reinterpret_cast<u64 (*)(u64, u64*)>(uintptr_t(create)); }

    ScriptableRenderer* UniversalRendererData::CreateRenderer() { return new UniversalRenderer(this); }

    // ---------------------------------------------------------------------------------------------- features

    bool ScriptableRendererFeature::IsCSharp() const { return IsCSharpScript(*this); }

    void ScriptableRendererFeature::OnEnable()
    {
        if (!IsCSharp())
            Create();
    }

    void ScriptableRendererFeature::Dispose()
    {
        u64 wrapper = scripting::ObjectHandles<engine::Object>::WrapperOf(this);
        if (IsCSharp() && gManagedFeature && wrapper)
            gManagedFeature(wrapper, 3, 0, 0);
        else
            Dispose(true);
    }

    void ScriptableRendererFeature::CallCreate()
    {
        u64 wrapper = scripting::ObjectHandles<engine::Object>::WrapperOf(this);
        if (IsCSharp() && gManagedFeature && wrapper)
            gManagedFeature(wrapper, 0, 0, 0);
        else
            Create();
    }

    void ScriptableRendererFeature::CallAddRenderPasses(ScriptableRenderer& renderer, RenderingData& renderingData)
    {
        u64 wrapper = scripting::ObjectHandles<engine::Object>::WrapperOf(this);
        if (IsCSharp() && gManagedFeature && wrapper)
            gManagedFeature(wrapper, 1, renderer.Id(), renderingData.frameData ? renderingData.frameData->Id() : 0);
        else
            AddRenderPasses(renderer, renderingData);
    }

    void ScriptableRendererFeature::CallOnCameraPreCull(ScriptableRenderer& renderer, const CameraData& cameraData)
    {
        u64 wrapper = scripting::ObjectHandles<engine::Object>::WrapperOf(this);
        if (IsCSharp() && gManagedFeature && wrapper)
            gManagedFeature(wrapper, 2, renderer.Id(), cameraData.frameData ? cameraData.frameData->Id() : 0);
        else
            OnCameraPreCull(renderer, cameraData);
    }

    void ScriptableRendererFeature::SetManaged(u64 call) { gManagedFeature = reinterpret_cast<void (*)(u64, i32, u64, u64)>(uintptr_t(call)); }

    // ---------------------------------------------------------------------------------------------- renderers

    ScriptableRenderer::ScriptableRenderer(ScriptableRendererData* data) : data_(data), dataVersion_(data ? data->version() : 0), id_(++R().next)
    {
        R().all.Set(id_, this);
    }

    ScriptableRenderer::~ScriptableRenderer()
    {
        for (u64& h : queueHandles_)
            scripting::ReleaseWrapper(h);
        scripting::ReleaseWrapper(wrapper_);
        R().all.Remove(id_);
    }

    ScriptableRenderer* ScriptableRenderer::Find(u64 id)
    {
        ScriptableRenderer* const* r = R().all.Find(id);
        return r ? *r : nullptr;
    }

    scripting::TypeId ScriptableRenderer::HelixType() const { return scripting::TypeOf<ScriptableRenderer>(); }

    void ScriptableRenderer::EnqueuePass(ScriptableRenderPass* pass)
    {
        if (!pass)
            return;
        queue_.Push(pass);
        queueHandles_.Push(scripting::RetainWrapper(pass->Wrapper()));
    }

    bool ScriptableRenderer::SupportsDevice()
    {
        engine::GraphicsDeviceType device = engine::SystemInfo::graphicsDeviceType();
        if (wrapper_ && gManagedDevice)
            if (u8 answer = gManagedDevice(wrapper_, i32(device)); answer != 2)
                return answer == 1;
        for (engine::GraphicsDeviceType t : unsupportedGraphicsDeviceTypes())
            if (t == device)
                return false;
        return true;
    }

    Array<ScriptableRenderPass*> ScriptableRenderer::activeRenderPassQueue() const
    {
        Array<ScriptableRenderPass*> out;
        out.Append(queue_.Data(), queue_.Size());
        return out;
    }

    Array<ScriptableRendererFeature*> ScriptableRenderer::rendererFeatures() const
    {
        Array<ScriptableRendererFeature*> out;
        if (ScriptableRendererData* d = data())
            for (ScriptableRendererFeature* f : d->rendererFeatures())
                if (f && f->isActive())
                    out.Push(f);
        return out;
    }

    void ScriptableRenderer::SetupCullingParameters(engine::ScriptableCullingParameters& parameters, const UniversalCameraData& cameraData)
    {
        (void)parameters, (void)cameraData;
    }

    void ScriptableRenderer::CallSetupCullingParameters(engine::ScriptableCullingParameters& parameters, ContextContainer& frameData)
    {
        if (wrapper_ && gManagedCulling && gManagedCulling(wrapper_, u64(uintptr_t(&parameters)), frameData.Id()))
            return;
        if (UniversalCameraData* c = frameData.Get<UniversalCameraData>())
            SetupCullingParameters(parameters, *c);
    }

    void ScriptableRenderer::SetupCullingParametersAt(u64 parameters, u64 frameData)
    {
        ContextContainer* frame = ContextContainer::FindContainer(frameData);
        UniversalCameraData* c = frame ? frame->Get<UniversalCameraData>() : nullptr;
        if (parameters && c)
            SetupCullingParameters(*reinterpret_cast<engine::ScriptableCullingParameters*>(uintptr_t(parameters)), *c);
    }

    void ScriptableRenderer::BaseSetupCullingParametersAt(u64 parameters, u64 frameData)
    {
        ContextContainer* frame = ContextContainer::FindContainer(frameData);
        UniversalCameraData* c = frame ? frame->Get<UniversalCameraData>() : nullptr;
        if (parameters && c)
            ScriptableRenderer::SetupCullingParameters(*reinterpret_cast<engine::ScriptableCullingParameters*>(uintptr_t(parameters)), *c);
    }

    void ScriptableRenderer::SetManaged(u64 setupCulling, u64 supportsDevice)
    {
        gManagedCulling = reinterpret_cast<u8 (*)(u64, u64, u64)>(uintptr_t(setupCulling));
        gManagedDevice = reinterpret_cast<u8 (*)(u64, i32)>(uintptr_t(supportsDevice));
    }

    ClearFlag ScriptableRenderer::GetCameraClearFlag(UniversalCameraData* cameraData)
    {
        if (!cameraData || !cameraData->camera)
            return ClearFlag::All;
        if (cameraData->renderType == CameraRenderType::Overlay)
            return ClearFlag::None;
        engine::CameraClearFlags flags = cameraData->camera->clearFlags();
        if (flags == engine::CameraClearFlags::Nothing)
            return ClearFlag::None;
        if (flags == engine::CameraClearFlags::Depth)
            return ClearFlag::DepthStencil;
        return ClearFlag::All;
    }

    void ScriptableRenderer::SetCameraMatrices(engine::CommandBuffer& cmd, const UniversalCameraData& cameraData, bool setInverseMatrices)
    {
        RenderingUtils::SetViewAndProjectionMatrices(cmd, cameraData.GetViewMatrix(), cameraData.GetProjectionMatrix(), setInverseMatrices);
    }

    void ScriptableRenderer::SetCameraMatricesOf(u64 cmd, UniversalCameraData* cameraData, bool setInverseMatrices)
    {
        if (engine::CommandBuffer* b = engine::CommandBuffer::Find(cmd); b && cameraData)
            SetCameraMatrices(*b, *cameraData, setInverseMatrices);
    }

    void RenderingUtils::SetViewAndProjectionMatrices(engine::CommandBuffer& cmd, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, bool setInverseMatrices)
    {
        cmd.SetViewProjectionMatrices(viewMatrix, projectionMatrix);
        if (!setInverseMatrices)
            return;
        // The inverse ones as the reference's shaders read them (the projection's GPU form of a texture target).
        Matrix4x4 gpuProjection = engine::GL::GetGPUProjectionMatrix(projectionMatrix, true);
        Matrix4x4 inverseView = viewMatrix.inverse();
        Matrix4x4 inverseProjection = gpuProjection.inverse();
        Matrix4x4 worldToCamera = Matrix4x4::Scale(Vector3(1, 1, -1)) * viewMatrix;
        cmd.SetGlobalMatrix(engine::Shader::PropertyToID("unity_WorldToCamera"), worldToCamera);
        cmd.SetGlobalMatrix(engine::Shader::PropertyToID("unity_CameraToWorld"), worldToCamera.inverse());
        cmd.SetGlobalMatrix(engine::Shader::PropertyToID("unity_MatrixInvV"), inverseView);
        cmd.SetGlobalMatrix(engine::Shader::PropertyToID("unity_MatrixInvP"), inverseProjection);
        cmd.SetGlobalMatrix(engine::Shader::PropertyToID("unity_MatrixInvVP"), inverseView * inverseProjection);
    }

    void RenderingUtils::SetViewAndProjectionMatricesOf(u64 cmd, Matrix4x4 viewMatrix, Matrix4x4 projectionMatrix, bool setInverseMatrices)
    {
        if (engine::CommandBuffer* b = engine::CommandBuffer::Find(cmd))
            SetViewAndProjectionMatrices(*b, viewMatrix, projectionMatrix, setInverseMatrices);
    }

    const ProfilingSampler& ScriptableRenderer::profilingExecute()
    {
        static const ProfilingSampler sampler("ScriptableRenderer.Execute");
        return sampler;
    }

    void ScriptableRenderer::Dispose()
    {
        for (u64& h : queueHandles_)
            scripting::ReleaseWrapper(h);
        queueHandles_.Clear();
        queue_.Clear();
    }

    void ScriptableRenderer::AddFeaturePasses(RenderingData& renderingData)
    {
        for (ScriptableRendererFeature* f : rendererFeatures())
            f->CallAddRenderPasses(*this, renderingData);
    }

    void ScriptableRenderer::CallOnCameraPreCull(const CameraData& cameraData)
    {
        for (ScriptableRendererFeature* f : rendererFeatures())
            f->CallOnCameraPreCull(*this, cameraData);
    }

    void ScriptableRenderer::SortPasses()
    {
        std::stable_sort(queue_.begin(), queue_.end(), [](const ScriptableRenderPass* a, const ScriptableRenderPass* b) {
            return i32(a->renderPassEvent()) < i32(b->renderPassEvent());
        });
    }

    void ScriptableRenderer::RecordPasses(RenderGraph& graph, ContextContainer& frameData, RenderPassEvent from, RenderPassEvent to)
    {
        for (ScriptableRenderPass* p : queue_)
            if (i32(p->renderPassEvent()) >= i32(from) && i32(p->renderPassEvent()) < i32(to))
                p->Record(graph, frameData);
    }

    void ScriptableRenderer::FinishFrame(engine::CommandBuffer& cmd)
    {
        for (ScriptableRenderPass* p : queue_)
            p->CameraCleanup(cmd);
        Dispose();
    }

    // ---------------------------------------------------------------------------------------------- the forward renderer

    UniversalRenderer::UniversalRenderer(UniversalRendererData* data) : ScriptableRenderer(data) { features_.msaa = true; }


    scripting::TypeId UniversalRenderer::HelixType() const { return scripting::TypeOf<UniversalRenderer>(); }

    void UniversalRenderer::SetupCullingParameters(engine::ScriptableCullingParameters& parameters, const UniversalCameraData& cameraData)
    {
        parameters.shadowDistance = cameraData.maxShadowDistance;
    }

    u64 UniversalRenderer::CreateRenderGraphTextureOf(u64 graph, u64 descriptor, StringView name, bool clear,
                                                      engine::FilterMode filterMode, engine::TextureWrapMode wrapMode)
    {
        RenderGraph* g = RenderGraph::Find(graph);
        if (!g || !descriptor)
            return 0;
        const auto& data = *reinterpret_cast<const engine::RenderTextureDescriptorData*>(uintptr_t(descriptor));
        TextureHandle h = CreateRenderGraphTexture(*g, data.ToDescriptor(), name, clear, filterMode, wrapMode);
        return u64(h.index) | (u64(h.execution) << 32);
    }

    TextureHandle UniversalRenderer::CreateRenderGraphTexture(RenderGraph& graph, const engine::RenderTextureDescriptor& descriptor, StringView name, bool clear,
                                                              engine::FilterMode filterMode, engine::TextureWrapMode wrapMode)
    {
        TextureDesc desc(descriptor.width, descriptor.height);
        desc.colorFormat = descriptor.graphicsFormat;
        desc.depthBufferBits = DepthBits(descriptor.depthBufferBits());
        if (descriptor.graphicsFormat == GraphicsFormat::None && desc.depthBufferBits == DepthBits::None)
            desc.depthBufferBits = DepthBits::Depth32;
        desc.msaaSamples = MSAASamples(descriptor.msaaSamples > 1 ? descriptor.msaaSamples : 1);
        desc.dimension = descriptor.dimension;
        desc.useMipMap = descriptor.useMipMap;
        desc.enableRandomWrite = descriptor.enableRandomWrite;
        desc.filterMode = filterMode;
        desc.wrapMode = wrapMode;
        desc.clearBuffer = clear;
        desc.name = String(name);
        return graph.CreateTexture(desc);
    }

    bool UniversalRenderer::IsOffscreenDepthTexture(const UniversalCameraData& cameraData)
    {
        engine::RenderTexture* t = cameraData.targetTexture;
        return t && t->descriptor().graphicsFormat == GraphicsFormat::None;
    }
}
