#pragma once

#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Rendering/Pipeline/RenderPipeline.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/RenderPipelines/Core/Volume/VolumeComponent.h>
#include <Helix/RenderPipelines/Standard/Renderer/ScriptableRenderer.h>

#include <memory>

// The Standard pipeline's asset: its settings and renderer data, set in GraphicsSettings or QualitySettings; the
// engine makes its pipeline in C++. Its C# class derives from RenderPipelineAsset.
namespace helix::rendering::universal
{
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", CSharpBase = "HelixEngine.Rendering.RenderPipelineAsset")
    class UniversalRenderPipelineAsset : public engine::ScriptableObject
    {
    public:
        // The sizes of a color grading lookup table (C# constants of the same names).
        static constexpr i32 k_MinLutSize = 16;
        static constexpr i32 k_MaxLutSize = 65;
        // The asset's file version: older files are upgraded when they are read.
        static constexpr i32 k_AssetVersion = 12;
        static constexpr i32 k_AssetPreviousVersion = 11;

        ~UniversalRenderPipelineAsset() override;

        // A new asset with one renderer data (a new UniversalRendererData when none is given).
        HX_FUNCTION() static UniversalRenderPipelineAsset* Create(ScriptableRendererData* rendererData = nullptr);

        // ------------------------------------------------------------------------------------------ renderers
        HX_PROPERTY(Name = "rendererDataArray", Internal) Array<ScriptableRendererData*> rendererDataList() const;
        void SetRendererDataList(Span<ScriptableRendererData* const> list, i32 defaultIndex = 0);
        i32 defaultRendererIndex() const { return defaultRendererIndex_; }
        // The renderer of a renderer data (made on first use, again when its data changed; -1: the default one).
        HX_FUNCTION() ScriptableRenderer* GetRenderer(i32 index);
        HX_PROPERTY(ReadOnly) ScriptableRenderer* scriptableRenderer() { return GetRenderer(-1); }
        HX_PROPERTY(Name = "renderersArray", Internal) Array<ScriptableRenderer*> renderers();

        // ------------------------------------------------------------------------------------------ quality
        HX_PROPERTY() bool supportsCameraDepthTexture() const { return requireDepthTexture_; }
        void SetSupportsCameraDepthTexture(bool value) { requireDepthTexture_ = value; }
        HX_PROPERTY() bool supportsCameraOpaqueTexture() const { return requireOpaqueTexture_; }
        void SetSupportsCameraOpaqueTexture(bool value) { requireOpaqueTexture_ = value; }
        HX_PROPERTY(ReadOnly) Downsampling opaqueDownsampling() const { return opaqueDownsampling_; }
        void SetOpaqueDownsampling(Downsampling value) { opaqueDownsampling_ = value; }
        HX_PROPERTY() bool supportsHDR() const { return supportsHDR_; }
        void SetSupportsHDR(bool value) { supportsHDR_ = value; }
        HX_PROPERTY() HDRColorBufferPrecision hdrColorBufferPrecision() const { return hdrPrecision_; }
        void SetHdrColorBufferPrecision(HDRColorBufferPrecision value) { hdrPrecision_ = value; }
        // 1, 2, 4 or 8 (other values take the nearest of them below).
        HX_PROPERTY() i32 msaaSampleCount() const { return i32(msaa_); }
        void SetMsaaSampleCount(i32 value);
        // 0.1 to 2.
        HX_PROPERTY() f32 renderScale() const { return renderScale_; }
        void SetRenderScale(f32 value);
        HX_PROPERTY() UpscalingFilterSelection upscalingFilter() const { return upscalingFilter_; }
        void SetUpscalingFilter(UpscalingFilterSelection value) { upscalingFilter_ = value; }

        // ------------------------------------------------------------------------------------------ lighting
        HX_PROPERTY(ReadOnly) LightRenderingMode mainLightRenderingMode() const { return mainLightRenderingMode_; }
        HX_PROPERTY(ReadOnly) bool supportsMainLightShadows() const { return mainLightShadowsSupported_; }
        HX_PROPERTY() i32 mainLightShadowmapResolution() const { return i32(mainLightShadowmapResolution_); }
        void SetMainLightShadowmapResolution(i32 value);
        HX_PROPERTY(ReadOnly) LightRenderingMode additionalLightsRenderingMode() const { return additionalLightsRenderingMode_; }
        // The additional lights per object (0 to UniversalRenderPipeline.maxPerObjectLights).
        HX_PROPERTY() i32 maxAdditionalLightsCount() const { return additionalLightsPerObjectLimit_; }
        void SetMaxAdditionalLightsCount(i32 value);

        // ------------------------------------------------------------------------------------------ shadows
        HX_PROPERTY() f32 shadowDistance() const { return shadowDistance_; }
        void SetShadowDistance(f32 value);
        // 1 to 4.
        HX_PROPERTY() i32 shadowCascadeCount() const { return shadowCascadeCount_; }
        void SetShadowCascadeCount(i32 value);
        HX_PROPERTY() f32 cascade2Split() const { return cascade2Split_; }
        void SetCascade2Split(f32 value) { cascade2Split_ = value; }
        HX_PROPERTY() Vector2 cascade3Split() const { return cascade3Split_; }
        void SetCascade3Split(Vector2 value) { cascade3Split_ = value; }
        HX_PROPERTY() Vector3 cascade4Split() const { return cascade4Split_; }
        void SetCascade4Split(Vector3 value) { cascade4Split_ = value; }
        // The part of the last cascade over which shadows fade out.
        HX_PROPERTY() f32 cascadeBorder() const { return cascadeBorder_; }
        void SetCascadeBorder(f32 value) { cascadeBorder_ = value; }
        HX_PROPERTY() f32 shadowDepthBias() const { return shadowDepthBias_; }
        void SetShadowDepthBias(f32 value);
        HX_PROPERTY() f32 shadowNormalBias() const { return shadowNormalBias_; }
        void SetShadowNormalBias(f32 value);
        HX_PROPERTY() bool supportsSoftShadows() const { return softShadowsSupported_; }
        void SetSupportsSoftShadows(bool value) { softShadowsSupported_ = value; }
        HX_PROPERTY() bool conservativeEnclosingSphere() const { return conservativeEnclosingSphere_; }
        void SetConservativeEnclosingSphere(bool value) { conservativeEnclosingSphere_ = value; }
        HX_PROPERTY() i32 numIterationsEnclosingSphere() const { return numIterationsEnclosingSphere_; }
        void SetNumIterationsEnclosingSphere(i32 value) { numIterationsEnclosingSphere_ = value; }
        // The soft shadow filtering of the main light (when its own setting leaves it to the pipeline).
        SoftShadowQuality softShadowQuality() const { return softShadowQuality_; }
        void SetSoftShadowQuality(SoftShadowQuality value) { softShadowQuality_ = value; }

        // ------------------------------------------------------------------------------------------ advanced
        HX_PROPERTY() bool useSRPBatcher() const { return useSRPBatcher_; }
        void SetUseSRPBatcher(bool value) { useSRPBatcher_ = value; }
        HX_PROPERTY() bool supportsDynamicBatching() const { return supportsDynamicBatching_; }
        void SetSupportsDynamicBatching(bool value) { supportsDynamicBatching_ = value; }
        HX_PROPERTY() StoreActionsOptimization storeActionsOptimization() const { return storeActionsOptimization_; }
        void SetStoreActionsOptimization(StoreActionsOptimization value) { storeActionsOptimization_ = value; }

        // ------------------------------------------------------------------------------------------ post processing
        HX_PROPERTY() ColorGradingMode colorGradingMode() const { return colorGradingMode_; }
        void SetColorGradingMode(ColorGradingMode value) { colorGradingMode_ = value; }
        // k_MinLutSize to k_MaxLutSize.
        HX_PROPERTY() i32 colorGradingLutSize() const { return colorGradingLutSize_; }
        void SetColorGradingLutSize(i32 value);
        HX_PROPERTY() bool useFastSRGBLinearConversion() const { return useFastSRGBLinearConversion_; }
        HX_PROPERTY() VolumeFrameworkUpdateMode volumeFrameworkUpdateMode() const { return volumeFrameworkUpdateMode_; }
        HX_PROPERTY() VolumeProfile* volumeProfile() const { return volumeProfile_.Get(); }
        void SetVolumeProfile(VolumeProfile* value) { volumeProfile_ = engine::PPtr<VolumeProfile>(value); }

        // ------------------------------------------------------------------------------------------ the engine's
        // The shaders and materials of new objects (Universal Render Pipeline/Lit; null until the pipeline's shaders load).
        HX_PROPERTY(Name = "defaultShaderNative", Internal) engine::Shader* defaultShader() const;
        HX_PROPERTY(Name = "defaultMaterialNative", Internal) engine::Material* defaultMaterial();
        // The material of new particle systems and of line and trail renderers (Universal Render Pipeline/Particles/Unlit).
        HX_PROPERTY(Name = "defaultParticleMaterialNative", Internal) engine::Material* defaultParticleMaterial();
        HX_PROPERTY(Name = "defaultLineMaterialNative", Internal) engine::Material* defaultLineMaterial() { return defaultParticleMaterial(); }
        HX_PROPERTY(ReadOnly) bool isImmediateModeSupported() const { return false; }
        HX_PROPERTY(ReadOnly) static String packagePath() { return String("Packages/Helix.RenderPipelines.Standard"); }

        engine::RenderPipeline* CreatePipeline();

        // Around its file form: an older file's fields upgraded to this version's (the renderer data of version 1,
        // the cascade option before version 6).
        HX_FUNCTION() void OnBeforeSerialize() {}
        HX_FUNCTION() void OnAfterDeserialize();

        // The global default volume profile of the pipeline's global settings (C# hands it over before the pipeline
        // is made; the asset's own profile is the quality default).
        HX_PROPERTY(Name = "globalVolumeProfileNative", Setter = "SetGlobalVolumeProfile", Internal) static VolumeProfile* globalVolumeProfile();
        static void SetGlobalVolumeProfile(VolumeProfile* profile);

    private:
        HX_FIELD(SerializeField, Name = "k_AssetVersion") i32 assetVersion_ = k_AssetVersion;
        HX_FIELD(SerializeField, Name = "k_AssetPreviousVersion") i32 assetPreviousVersion_ = k_AssetPreviousVersion;
        // Fields of older files (read for the upgrade; the reference keeps them in its file form too).
        HX_FIELD(SerializeField, Name = "m_RendererData") engine::PPtr<ScriptableRendererData> legacyRendererData_;
        HX_FIELD(SerializeField, Name = "m_ShadowCascades") i32 legacyShadowCascades_ = 0;
        HX_FIELD(SerializeField, Name = "m_RendererDataList") Array<engine::PPtr<ScriptableRendererData>> rendererData_;
        HX_FIELD(SerializeField, Name = "m_DefaultRendererIndex") i32 defaultRendererIndex_ = 0;
        HX_FIELD(SerializeField, Name = "m_RequireDepthTexture") bool requireDepthTexture_ = false;
        HX_FIELD(SerializeField, Name = "m_RequireOpaqueTexture") bool requireOpaqueTexture_ = false;
        HX_FIELD(SerializeField, Name = "m_OpaqueDownsampling") Downsampling opaqueDownsampling_ = Downsampling::_2xBilinear;
        HX_FIELD(SerializeField, Name = "m_SupportsHDR") bool supportsHDR_ = true;
        HX_FIELD(SerializeField, Name = "m_HDRColorBufferPrecision") HDRColorBufferPrecision hdrPrecision_ = HDRColorBufferPrecision::_32Bits;
        HX_FIELD(SerializeField, Name = "m_MSAA") MsaaQuality msaa_ = MsaaQuality::Disabled;
        HX_FIELD(SerializeField, Name = "m_RenderScale") f32 renderScale_ = 1;
        HX_FIELD(SerializeField, Name = "m_UpscalingFilter") UpscalingFilterSelection upscalingFilter_ = UpscalingFilterSelection::Auto;
        HX_FIELD(SerializeField, Name = "m_MainLightRenderingMode") LightRenderingMode mainLightRenderingMode_ = LightRenderingMode::PerPixel;
        HX_FIELD(SerializeField, Name = "m_MainLightShadowsSupported") bool mainLightShadowsSupported_ = true;
        HX_FIELD(SerializeField, Name = "m_MainLightShadowmapResolution") ShadowResolution mainLightShadowmapResolution_ = ShadowResolution::_2048;
        HX_FIELD(SerializeField, Name = "m_AdditionalLightsRenderingMode") LightRenderingMode additionalLightsRenderingMode_ = LightRenderingMode::PerPixel;
        HX_FIELD(SerializeField, Name = "m_AdditionalLightsPerObjectLimit") i32 additionalLightsPerObjectLimit_ = 4;
        HX_FIELD(SerializeField, Name = "m_ShadowDistance") f32 shadowDistance_ = 50;
        HX_FIELD(SerializeField, Name = "m_ShadowCascadeCount") i32 shadowCascadeCount_ = 1;
        HX_FIELD(SerializeField, Name = "m_Cascade2Split") f32 cascade2Split_ = 0.25f;
        HX_FIELD(SerializeField, Name = "m_Cascade3Split") Vector2 cascade3Split_{0.1f, 0.3f};
        HX_FIELD(SerializeField, Name = "m_Cascade4Split") Vector3 cascade4Split_{0.067f, 0.2f, 0.467f};
        HX_FIELD(SerializeField, Name = "m_CascadeBorder") f32 cascadeBorder_ = 0.2f;
        HX_FIELD(SerializeField, Name = "m_ShadowDepthBias") f32 shadowDepthBias_ = 1;
        HX_FIELD(SerializeField, Name = "m_ShadowNormalBias") f32 shadowNormalBias_ = 1;
        HX_FIELD(SerializeField, Name = "m_SoftShadowsSupported") bool softShadowsSupported_ = false;
        HX_FIELD(SerializeField, Name = "m_ConservativeEnclosingSphere") bool conservativeEnclosingSphere_ = false;
        HX_FIELD(SerializeField, Name = "m_NumIterationsEnclosingSphere") i32 numIterationsEnclosingSphere_ = 64;
        HX_FIELD(SerializeField, Name = "m_SoftShadowQuality") SoftShadowQuality softShadowQuality_ = SoftShadowQuality::Medium;
        HX_FIELD(SerializeField, Name = "m_UseSRPBatcher") bool useSRPBatcher_ = true;
        HX_FIELD(SerializeField, Name = "m_SupportsDynamicBatching") bool supportsDynamicBatching_ = false;
        HX_FIELD(SerializeField, Name = "m_StoreActionsOptimization") StoreActionsOptimization storeActionsOptimization_ = StoreActionsOptimization::Auto;
        HX_FIELD(SerializeField, Name = "m_ColorGradingMode") ColorGradingMode colorGradingMode_ = ColorGradingMode::LowDynamicRange;
        HX_FIELD(SerializeField, Name = "m_ColorGradingLutSize") i32 colorGradingLutSize_ = 32;
        HX_FIELD(SerializeField, Name = "m_UseFastSRGBLinearConversion") bool useFastSRGBLinearConversion_ = false;
        HX_FIELD(SerializeField, Name = "m_VolumeFrameworkUpdateMode") VolumeFrameworkUpdateMode volumeFrameworkUpdateMode_ = VolumeFrameworkUpdateMode::EveryFrame;
        HX_FIELD(SerializeField, Name = "m_VolumeProfile") engine::PPtr<VolumeProfile> volumeProfile_;

        // The renderers of the renderer data, by index (a copy of the asset makes its own), with the strong handles of
        // the C# objects of C# renderers (0 for C++ ones).
        struct Renderers
        {
            Array<std::unique_ptr<ScriptableRenderer>> list = Array<std::unique_ptr<ScriptableRenderer>>();
            Array<u64> handles = Array<u64>();
            Renderers() = default;
            Renderers(const Renderers&) {}
            Renderers& operator=(const Renderers&)
            {
                Clear();
                return *this;
            }
            ~Renderers() { Clear(); }
            void Clear();
            void Set(u32 index, ScriptableRenderer* renderer, u64 handle);
        };
        Renderers renderers_;
        engine::PPtr<engine::Material> defaultMaterial_, defaultParticleMaterial_;
    };
}
