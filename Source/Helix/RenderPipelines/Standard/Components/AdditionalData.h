#pragma once

#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Light.h>
#include <Helix/Engine/Script/GameBehaviour.h>
#include <Helix/RenderPipelines/Core/Volume/VolumeComponent.h>
#include <Helix/RenderPipelines/Standard/Renderer/RendererData.h>

// The pipeline's settings of a camera and of a light: components beside them.
namespace helix::rendering::universal
{
    class ScriptableRenderer;

    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalAdditionalCameraData : public engine::GameBehaviour
    {
    public:
        ~UniversalAdditionalCameraData() override;

        HX_PROPERTY() bool renderShadows() const { return renderShadows_; }
        void SetRenderShadows(bool value) { renderShadows_ = value; }
        HX_PROPERTY() CameraOverrideOption requiresDepthOption() const { return requiresDepthOption_; }
        void SetRequiresDepthOption(CameraOverrideOption value) { requiresDepthOption_ = value; }
        HX_PROPERTY() CameraOverrideOption requiresColorOption() const { return requiresColorOption_; }
        void SetRequiresColorOption(CameraOverrideOption value) { requiresColorOption_ = value; }
        HX_PROPERTY() CameraRenderType renderType() const { return renderType_; }
        void SetRenderType(CameraRenderType value) { renderType_ = value; }
        // Whether the camera makes a depth (color) texture: its option, or the asset's when the option leaves it to the
        // pipeline. Setting it sets the option On or Off.
        HX_PROPERTY() bool requiresDepthTexture() const;
        void SetRequiresDepthTexture(bool value) { requiresDepthOption_ = value ? CameraOverrideOption::On : CameraOverrideOption::Off; }
        HX_PROPERTY() bool requiresColorTexture() const;
        void SetRequiresColorTexture(bool value) { requiresColorOption_ = value ? CameraOverrideOption::On : CameraOverrideOption::Off; }
        // The renderer data the camera draws with: its index in the asset's list (-1: the asset's default).
        HX_FUNCTION() void SetRenderer(i32 index) { rendererIndex_ = index; }
        i32 rendererIndex() const { return rendererIndex_; }
        // The renderer of that data (null without the pipeline's asset).
        HX_PROPERTY(ReadOnly) ScriptableRenderer* scriptableRenderer() const;
        HX_PROPERTY(Name = "volumeLayerMaskBits", Setter = "SetVolumeLayerMaskBits", Internal) i32 volumeLayerMaskBits() const { return i32(volumeLayerMask_.bits); }
        void SetVolumeLayerMaskBits(i32 value) { volumeLayerMask_.bits = u32(value); }
        // Where the volumes are blended for (null: the camera).
        HX_PROPERTY() engine::Transform* volumeTrigger() const { return volumeTrigger_.Get(); }
        void SetVolumeTrigger(engine::Transform* value) { volumeTrigger_ = engine::PPtr<engine::Transform>(value); }
        // Whether the volumes are blended for this camera every frame (else when the script calls the update).
        HX_PROPERTY(ReadOnly) bool requiresVolumeFrameworkUpdate() const;
        VolumeFrameworkUpdateMode volumeFrameworkUpdateMode() const { return volumeUpdateMode_; }
        void SetVolumeFrameworkUpdateMode(VolumeFrameworkUpdateMode value) { volumeUpdateMode_ = value; }
        // The camera's own volume stack (made when the volumes are blended for it alone; null until then).
        VolumeStack* volumeStack() const { return VolumeStack::Find(volumeStack_); }
        void SetVolumeStack(VolumeStack* stack) { volumeStack_ = stack ? stack->Id() : 0; }
        HX_PROPERTY(Name = "volumeStackId", Setter = "SetVolumeStackId", Internal) u64 volumeStackId() const { return volumeStack_; }
        void SetVolumeStackId(u64 id) { volumeStack_ = id; }
        HX_PROPERTY() bool renderPostProcessing() const { return renderPostProcessing_; }
        void SetRenderPostProcessing(bool value) { renderPostProcessing_ = value; }
        HX_PROPERTY(ReadOnly) i32 version() const { return version_; }

        HX_FUNCTION() void OnBeforeSerialize() {}
        HX_FUNCTION() void OnAfterDeserialize() {}
        // Its own volume stack goes.
        HX_FUNCTION() void OnDestroy();

    private:
        HX_FIELD(SerializeField, Name = "m_RenderShadows") bool renderShadows_ = true;
        HX_FIELD(SerializeField, Name = "m_RequiresDepthTextureOption") CameraOverrideOption requiresDepthOption_ = CameraOverrideOption::UsePipelineSettings;
        HX_FIELD(SerializeField, Name = "m_RequiresOpaqueTextureOption") CameraOverrideOption requiresColorOption_ = CameraOverrideOption::UsePipelineSettings;
        HX_FIELD(SerializeField, Name = "m_CameraType") CameraRenderType renderType_ = CameraRenderType::Base;
        HX_FIELD(SerializeField, Name = "m_RendererIndex") i32 rendererIndex_ = -1;
        HX_FIELD(SerializeField, Name = "m_VolumeLayerMask") LayerMaskBits volumeLayerMask_{1};
        HX_FIELD(SerializeField, Name = "m_VolumeTrigger") engine::PPtr<engine::Transform> volumeTrigger_;
        HX_FIELD(SerializeField, Name = "m_VolumeFrameworkUpdateModeOption") VolumeFrameworkUpdateMode volumeUpdateMode_ = VolumeFrameworkUpdateMode::UsePipelineSettings;
        HX_FIELD(SerializeField, Name = "m_RenderPostProcessing") bool renderPostProcessing_ = false;
        HX_FIELD(SerializeField, Name = "m_Version") i32 version_ = 2;
        u64 volumeStack_ = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class UniversalAdditionalLightData : public engine::GameBehaviour
    {
    public:
        // Whether the light's shadow settings are the asset's (its bias, soft shadow quality).
        HX_PROPERTY() bool usePipelineSettings() const { return usePipelineSettings_; }
        void SetUsePipelineSettings(bool value) { usePipelineSettings_ = value; }
        HX_PROPERTY() SoftShadowQuality softShadowQuality() const { return softShadowQuality_; }
        void SetSoftShadowQuality(SoftShadowQuality value) { softShadowQuality_ = value; }
        // The data's format version (internal in C#, as the reference's).
        i32 version() const { return version_; }

        HX_FUNCTION() void OnBeforeSerialize() {}
        HX_FUNCTION() void OnAfterDeserialize() {}

    private:
        HX_FIELD(SerializeField, Name = "m_UsePipelineSettings") bool usePipelineSettings_ = true;
        HX_FIELD(SerializeField, Name = "m_SoftShadowQuality") SoftShadowQuality softShadowQuality_ = SoftShadowQuality::UsePipelineSettings;
        HX_FIELD(SerializeField, Name = "m_Version") i32 version_ = 4;
    };

    // The pipeline's settings of a camera or a light: the component beside it (added when it has none).
    UniversalAdditionalCameraData* GetUniversalAdditionalCameraData(engine::Camera* camera);
    UniversalAdditionalLightData* GetUniversalAdditionalLightData(engine::Light* light);

    // What C#'s CameraExtensions and LightExtensions call.
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Static, Internal)
    class AdditionalDataNative
    {
    public:
        HX_FUNCTION() static UniversalAdditionalCameraData* OfCamera(engine::Camera* camera) { return GetUniversalAdditionalCameraData(camera); }
        HX_FUNCTION() static UniversalAdditionalLightData* OfLight(engine::Light* light) { return GetUniversalAdditionalLightData(light); }
    };
}
