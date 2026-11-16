#pragma once

#include <Helix/Engine/Rendering/SystemInfo.h>
#include <Helix/RenderPipelines/Core/Debugging/ProfilingSampler.h>
#include <Helix/RenderPipelines/Core/PostProcessing/PostProcessing.h>
#include <Helix/RenderPipelines/Standard/Frame/FrameData.h>
#include <Helix/RenderPipelines/Standard/Passes/PostProcessPass.h>
#include <Helix/RenderPipelines/Standard/Passes/ScriptableRenderPass.h>
#include <Helix/RenderPipelines/Standard/Renderer/RendererData.h>

// The renderers: what draws a camera, made from renderer data (the frame itself in the classes below). Passes are
// queued for a camera's frame (the features' and the renderer's own) and recorded in the order of their events. C#
// classes below ScriptableRenderer are renderers too (their objects owned by their C# objects; the asset that keeps
// one keeps its C# object alive).
namespace helix::rendering::universal
{
    // What a renderer clears of the camera's target.
    HX_ENUM(Namespace = "HelixEngine.Rendering.Universal", Attributes = "[System.Flags]")
    enum class ClearFlag : i32
    {
        None = 0,
        Color = 1,
        Depth = 2,
        Stencil = 4,
        DepthStencil = Depth | Stencil,
        ColorStencil = Color | Stencil,
        All = Color | Depth | Stencil,
    };

    HX_CLASS(Root, Owned, Namespace = "HelixEngine.Rendering.Universal")
    class ScriptableRenderer
    {
    public:
        // What the renderer can do with a camera.
        HX_STRUCT(Namespace = "HelixEngine.Rendering.Universal")
        struct RenderingFeatures
        {
            HX_FIELD() bool cameraStacking = false;
            HX_FIELD() bool msaa = true;
        };

        HX_FUNCTION(Constructor) explicit ScriptableRenderer(ScriptableRendererData* data);
        virtual ~ScriptableRenderer();
        ScriptableRenderer(const ScriptableRenderer&) = delete;
        ScriptableRenderer& operator=(const ScriptableRenderer&) = delete;

        // The data it was made from (null when the data is gone) and the data's version then.
        ScriptableRendererData* data() const { return data_.Get(); }
        u32 dataVersion() const { return dataVersion_; }

        // A pass for the camera's frame (the passes go when the frame ends; a C# pass's C# object lives until then).
        HX_FUNCTION() void EnqueuePass(ScriptableRenderPass* pass);
        HX_PROPERTY(Name = "activeRenderPassQueueArray", Internal) Array<ScriptableRenderPass*> activeRenderPassQueue() const;
        // The data's active features.
        HX_PROPERTY(Name = "rendererFeaturesArray", Internal) Array<ScriptableRendererFeature*> rendererFeatures() const;
        HX_PROPERTY(ReadOnly) RenderingFeatures supportedRenderingFeatures() const { return features_; }
        HX_FUNCTION() virtual bool SupportsCameraOpaque() { return true; }
        HX_FUNCTION() virtual bool SupportsCameraNormals() { return false; }
        // Around the recording of a frame's graph (a renderer may queue its own passes in OnBeginRenderGraphFrame).
        HX_FUNCTION() virtual void OnBeginRenderGraphFrame() {}
        HX_FUNCTION() virtual void OnEndRenderGraphFrame() {}
        // The device types the renderer does not draw on (a camera that uses it then renders nothing, with an error);
        // a C# renderer's C# property.
        virtual Array<engine::GraphicsDeviceType> unsupportedGraphicsDeviceTypes() { return Array<engine::GraphicsDeviceType>(); }
        HX_FUNCTION(Name = "UnsupportedGraphicsDeviceTypesNative", Internal) Array<engine::GraphicsDeviceType> UnsupportedGraphicsDeviceTypesNative()
        {
            return unsupportedGraphicsDeviceTypes();
        }
        bool SupportsDevice();
        // The culling of a camera (the camera's own parameters changed by the renderer's settings).
        virtual void SetupCullingParameters(engine::ScriptableCullingParameters& parameters, const UniversalCameraData& cameraData);
        // The pipeline's call: a C# renderer's C# override, else the C++ one.
        void CallSetupCullingParameters(engine::ScriptableCullingParameters& parameters, ContextContainer& frameData);
        // What C# calls: the parameters at a native address, the camera data of the frame data's id (the virtual, and
        // ScriptableRenderer's own for the base calls of C# renderers).
        HX_FUNCTION(Name = "SetupCullingParametersNative", Internal) void SetupCullingParametersAt(u64 parameters, u64 frameData);
        HX_FUNCTION(Name = "BaseSetupCullingParameters", Internal) void BaseSetupCullingParametersAt(u64 parameters, u64 frameData);
        // C#'s functions for C# renderers: of (the C# object's GC handle, the parameters' address, the frame data's id),
        // answering whether the renderer is of a C# class; and of (the GC handle, the device type), answering 0 when
        // the device is not taken, 1 when it is, 2 for a renderer of no C# class.
        HX_FUNCTION(Internal) static void SetManaged(u64 setupCulling, u64 supportsDevice);
        // What a camera clears: everything for a base camera that clears to a color or the skybox, depth only for one
        // that clears depth, nothing else.
        HX_FUNCTION() static ClearFlag GetCameraClearFlag(UniversalCameraData* cameraData);
        // The camera's matrices as the shaders read them (the GPU projection for the current target), with the
        // inverse ones (unity_MatrixInvV, unity_MatrixInvP, unity_MatrixInvVP, unity_WorldToCamera, unity_CameraToWorld)
        // when asked.
        static void SetCameraMatrices(engine::CommandBuffer& cmd, const UniversalCameraData& cameraData, bool setInverseMatrices);
        HX_FUNCTION(Name = "SetCameraMatricesNative", Internal) static void SetCameraMatricesOf(u64 cmd, UniversalCameraData* cameraData, bool setInverseMatrices);
        static const ProfilingSampler& profilingExecute();
        HX_FUNCTION(Name = "DisposeNative", Internal) void Dispose();

        // The frame: the features' passes queued (their AddRenderPasses), the queue sorted by event (stable), the
        // passes recorded, the queue emptied and the passes' camera cleanup.
        void AddFeaturePasses(RenderingData& renderingData);
        void CallOnCameraPreCull(const CameraData& cameraData);
        void SortPasses();
        Span<ScriptableRenderPass* const> passes() const { return Span<ScriptableRenderPass* const>(queue_.Data(), queue_.Size()); }
        void RecordPasses(RenderGraph& graph, ContextContainer& frameData, RenderPassEvent from, RenderPassEvent to);
        void FinishFrame(engine::CommandBuffer& cmd);
        // A camera's frame on the graph (the frame data filled, the camera culled), executed into cmd. This one: the
        // camera's color and depth, the queued passes by event, the final blit into the camera's target.
        virtual void RenderFrame(engine::ScriptableRenderContext& context, RenderGraph& graph, PostProcessPasses& post, ContextContainer& frame,
                                 engine::CommandBuffer& cmd);

        u64 Id() const { return id_; }
        u64& Wrapper() { return wrapper_; }
        static ScriptableRenderer* Find(u64 id);
        virtual scripting::TypeId HelixType() const;

    protected:
        RenderingFeatures features_;

    private:
        engine::PPtr<ScriptableRendererData> data_;
        u32 dataVersion_ = 0;
        Array<ScriptableRenderPass*> queue_ = Array<ScriptableRenderPass*>();
        // The strong handles of the queued passes' C# objects (0 for C++ passes).
        Array<u64> queueHandles_ = Array<u64>();
        u64 id_ = 0;
        u64 wrapper_ = 0;
    };

    // The forward renderer (its frame: UniversalRenderer.cpp).
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Sealed)
    class UniversalRenderer final : public ScriptableRenderer
    {
    public:
        HX_FUNCTION(Constructor) explicit UniversalRenderer(UniversalRendererData* data);
        ~UniversalRenderer() override;

        UniversalRendererData* rendererData() const { return static_cast<UniversalRendererData*>(data()); }
        bool SupportsCameraOpaque() override { return true; }
        bool SupportsCameraNormals() override { return false; }
        void SetupCullingParameters(engine::ScriptableCullingParameters& parameters, const UniversalCameraData& cameraData) override;
        // A texture of the graph like the camera's descriptor (a depth texture when it has a depth format).
        static TextureHandle CreateRenderGraphTexture(RenderGraph& graph, const engine::RenderTextureDescriptor& descriptor, StringView name,
                                                      bool clear, engine::FilterMode filterMode = engine::FilterMode::Point,
                                                      engine::TextureWrapMode wrapMode = engine::TextureWrapMode::Clamp);
        // For C#: the graph by its id, the descriptor's plain form at a native address, the handle packed.
        HX_FUNCTION(Name = "CreateRenderGraphTextureNative", Internal) static u64 CreateRenderGraphTextureOf(u64 graph, u64 descriptor,
                                                                                                         StringView name, bool clear, engine::FilterMode filterMode,
                                                                                                         engine::TextureWrapMode wrapMode);
        // Whether the camera draws only depth into its target (a depth only render texture).
        static bool IsOffscreenDepthTexture(const UniversalCameraData& cameraData);
        HX_FUNCTION() static bool IsOffscreenDepthTexture(UniversalCameraData* cameraData) { return cameraData && IsOffscreenDepthTexture(*cameraData); }
        scripting::TypeId HelixType() const override;

        // A camera's frame on the graph (the frame data filled and the camera culled): the main light's shadows,
        // opaque, skybox, transparent, the post process injection points, the final blit into the camera's target,
        // the passes of the features at their events; executed into cmd.
        void RenderFrame(engine::ScriptableRenderContext& context, RenderGraph& graph, PostProcessPasses& post, ContextContainer& frame,
                         engine::CommandBuffer& cmd) override;

    private:
        // The materials of the depth copy and of the color copy's box filter (made on first use).
        void EnsureMaterials();
        // The main light's cascades into the shadow atlas, their globals.
        void RecordMainLightShadows(engine::ScriptableRenderContext& context, RenderGraph& graph, ContextContainer& frame, engine::CommandBuffer& cmd);

        // Held by id: the player's teardown may destroy them before the renderer goes.
        engine::PPtr<engine::Material> copyDepth_, sampling_;
        // The post processing passes (their materials made on first use).
        ColorGradingLutPass lutPass_;
        PostProcessPass postPass_;
    };

    // Helpers of passes and renderers (the C# side adds CreateDrawingSettings and ReAllocateHandleIfNeeded).
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Static)
    class RenderingUtils
    {
    public:
        // Whether the device makes render textures of the format.
        HX_FUNCTION() static bool SupportsRenderTextureFormat(engine::RenderTextureFormat format) { return engine::SystemInfo::SupportsRenderTextureFormat(format); }
        // The view and projection of the draws after it (the projection's GPU form for the current target), with the
        // inverse ones when asked.
        static void SetViewAndProjectionMatrices(engine::CommandBuffer& cmd, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, bool setInverseMatrices);
        HX_FUNCTION(Name = "SetViewAndProjectionMatricesNative", Internal) static void SetViewAndProjectionMatricesOf(u64 cmd, Matrix4x4 viewMatrix,
                                                                                                                 Matrix4x4 projectionMatrix, bool setInverseMatrices);
    };
}

namespace helix::rendering::universal
{
    // What C# calls (renderers by id).
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Static, Internal)
    class ScriptableRendererNative
    {
    public:
        HX_FUNCTION() static ScriptableRenderer* Of(u64 id) { return ScriptableRenderer::Find(id); }
    };
}

template <>
struct helix::scripting::ObjectHandles<helix::rendering::universal::ScriptableRenderer>
{
    static u64 ToId(const rendering::universal::ScriptableRenderer* r) { return r->Id(); }
    static rendering::universal::ScriptableRenderer* FromId(u64 id) { return rendering::universal::ScriptableRenderer::Find(id); }
    static TypeId TypeOf(const rendering::universal::ScriptableRenderer* r) { return r->HelixType(); }
    static u64& WrapperOf(rendering::universal::ScriptableRenderer* r) { return r->Wrapper(); }
};
