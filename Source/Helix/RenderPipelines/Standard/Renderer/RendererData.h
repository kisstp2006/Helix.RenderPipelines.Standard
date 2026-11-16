#pragma once

#include <Helix/Engine/Object/Object.h>
#include <Helix/Engine/Rendering/Pipeline/RenderStates.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/RenderPipelines/Standard/Renderer/PostProcessData.h>
#include <Helix/RenderPipelines/Standard/Renderer/ScriptableRendererFeature.h>
#include <Helix/RenderPipelines/Standard/Settings/PipelineSettings.h>

#include <memory>

// The data a renderer is made from: assets the pipeline asset lists; the renderer each makes draws the cameras that
// use it.
namespace helix::rendering::universal
{
    class ScriptableRenderer;

    // A layer mask in files: its bits (serializedVersion 2).
    HX_STRUCT(Namespace = "HelixEngine.Rendering.Universal", Internal)
    struct LayerMaskBits
    {
        static constexpr StringView kTypeName = "BitField";
        static constexpr i32 kVersion = 2;
        HX_FIELD(Name = "m_Bits") u32 bits = ~0u;

        template <class V>
        void Transfer(V& v)
        {
            v.Field("m_Bits", bits);
        }
    };

    // The stencil state the renderer's opaque and transparent draws take when it overrides theirs.
    HX_STRUCT(Namespace = "HelixEngine.Rendering.Universal")
    struct StencilStateData
    {
        HX_FIELD() bool overrideStencilState = false;
        HX_FIELD() i32 stencilReference = 0;
        HX_FIELD() engine::CompareFunction stencilCompareFunction = engine::CompareFunction::Always;
        HX_FIELD() engine::StencilOp passOperation = engine::StencilOp::Keep;
        HX_FIELD() engine::StencilOp failOperation = engine::StencilOp::Keep;
        HX_FIELD() engine::StencilOp zFailOperation = engine::StencilOp::Keep;

        template <class V>
        void Transfer(V& v)
        {
            v.Field("overrideStencilState", overrideStencilState);
            v.Field("stencilReference", stencilReference);
            v.Field("stencilCompareFunction", stencilCompareFunction);
            v.Field("passOperation", passOperation);
            v.Field("failOperation", failOperation);
            v.Field("zFailOperation", zFailOperation);
        }
    };

    // Renderer data of any kind: its features; the renderer it makes (Create, in the classes below).
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class ScriptableRendererData : public engine::ScriptableObject
    {
    public:
        ~ScriptableRendererData() override;

        HX_PROPERTY(Name = "rendererFeaturesArray", Setter = "SetRendererFeatures", Internal) Array<ScriptableRendererFeature*> rendererFeatures() const;
        void SetRendererFeatures(Array<ScriptableRendererFeature*> features);
        // The first active or inactive feature of a type (null: none).
        ScriptableRendererFeature* FindRendererFeature(scripting::TypeId type) const;
        HX_PROPERTY() bool useNativeRenderPass() const { return useNativeRenderPass_; }
        void SetUseNativeRenderPass(bool value);
        // Its renderers are made again (the features or settings changed).
        HX_FUNCTION(Name = "SetDirtyNative", Internal) void SetDirty() { version_++; }
        u32 version() const { return version_; }

        // A new renderer of this data (the pipeline asset keeps it; data of no kind makes none).
        virtual ScriptableRenderer* CreateRenderer() { return nullptr; }
        // Resources a data of a class fills in when it is enabled without them.
        virtual void EnsureResources() {}
        // The asset's call: a C# class's Create (C#'s renderer, with a strong handle of its C# object in handle), else
        // CreateRenderer.
        ScriptableRenderer* CallCreateRenderer(u64& handle);
        // C#'s function for C# data: of (the C# object's GC handle, where the renderer's strong handle goes); it answers
        // the renderer's id (0: none).
        HX_FUNCTION(Internal) static void SetManaged(u64 create);

    protected:
        bool IsCSharp() const;

    private:
        // Enabled or changed: its renderers are made again (C# classes do this on their side).
        void OnEnable();
        void OnValidate();

        HX_FIELD(SerializeField, Name = "m_RendererFeatures") Array<engine::PPtr<ScriptableRendererFeature>> features_;
        HX_FIELD(SerializeField, Name = "m_RendererFeatureMap") Array<i64> featureMap_;
        HX_FIELD(SerializeField, Name = "m_UseNativeRenderPass") bool useNativeRenderPass_ = false;
        u32 version_ = 0;
    };

    // The data of the forward renderer.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalRendererData : public ScriptableRendererData
    {
    public:
        HX_PROPERTY(Name = "opaqueLayerMaskBits", Setter = "SetOpaqueLayerMaskBits", Internal) i32 opaqueLayerMaskBits() const { return i32(opaqueLayerMask_.bits); }
        void SetOpaqueLayerMaskBits(i32 value) { opaqueLayerMask_.bits = u32(value), SetDirty(); }
        HX_PROPERTY(Name = "transparentLayerMaskBits", Setter = "SetTransparentLayerMaskBits", Internal) i32 transparentLayerMaskBits() const
        {
            return i32(transparentLayerMask_.bits);
        }
        void SetTransparentLayerMaskBits(i32 value) { transparentLayerMask_.bits = u32(value), SetDirty(); }
        HX_PROPERTY() StencilStateData defaultStencilState() const { return defaultStencilState_; }
        void SetDefaultStencilState(StencilStateData value) { defaultStencilState_ = value, SetDirty(); }
        // Whether transparent objects receive the main light's shadows.
        HX_PROPERTY() bool shadowTransparentReceive() const { return shadowTransparentReceive_; }
        void SetShadowTransparentReceive(bool value) { shadowTransparentReceive_ = value, SetDirty(); }
        HX_PROPERTY() RenderingMode renderingMode() const { return renderingMode_; }
        void SetRenderingMode(RenderingMode value) { renderingMode_ = value, SetDirty(); }
        HX_PROPERTY() CopyDepthMode copyDepthMode() const { return copyDepthMode_; }
        void SetCopyDepthMode(CopyDepthMode value) { copyDepthMode_ = value, SetDirty(); }
        HX_PROPERTY() DepthFormat depthAttachmentFormat() const { return depthAttachmentFormat_; }
        void SetDepthAttachmentFormat(DepthFormat value) { depthAttachmentFormat_ = value, SetDirty(); }
        HX_PROPERTY() DepthFormat depthTextureFormat() const { return depthTextureFormat_; }
        void SetDepthTextureFormat(DepthFormat value) { depthTextureFormat_ = value, SetDirty(); }
        HX_PROPERTY() IntermediateTextureMode intermediateTextureMode() const { return intermediateTextureMode_; }
        void SetIntermediateTextureMode(IntermediateTextureMode value) { intermediateTextureMode_ = value, SetDirty(); }
        // The post processing shaders and textures (none: no post processing).
        HX_PROPERTY() PostProcessData* postProcessData() const { return postProcessData_.Get(); }
        void SetPostProcessData(PostProcessData* value) { postProcessData_ = engine::PPtr<PostProcessData>(value), SetDirty(); }

        ScriptableRenderer* CreateRenderer() override;
        // Without post processing data the default one (the pipeline's shaders).
        void EnsureResources() override;

    private:
        HX_FIELD(SerializeField, Name = "m_OpaqueLayerMask") LayerMaskBits opaqueLayerMask_;
        HX_FIELD(SerializeField, Name = "m_TransparentLayerMask") LayerMaskBits transparentLayerMask_;
        HX_FIELD(SerializeField, Name = "m_DefaultStencilState") StencilStateData defaultStencilState_;
        HX_FIELD(SerializeField, Name = "m_ShadowTransparentReceive") bool shadowTransparentReceive_ = true;
        HX_FIELD(SerializeField, Name = "m_RenderingMode") RenderingMode renderingMode_ = RenderingMode::Forward;
        HX_FIELD(SerializeField, Name = "m_CopyDepthMode") CopyDepthMode copyDepthMode_ = CopyDepthMode::AfterOpaques;
        HX_FIELD(SerializeField, Name = "m_DepthAttachmentFormat") DepthFormat depthAttachmentFormat_ = DepthFormat::Default;
        HX_FIELD(SerializeField, Name = "m_DepthTextureFormat") DepthFormat depthTextureFormat_ = DepthFormat::Default;
        HX_FIELD(SerializeField, Name = "m_IntermediateTextureMode") IntermediateTextureMode intermediateTextureMode_ = IntermediateTextureMode::Auto;
        HX_FIELD(SerializeField, Name = "m_PostProcessData") engine::PPtr<PostProcessData> postProcessData_;
    };
}
