#include <Helix/RenderPipelines/Standard/Components/AdditionalData.h>

#include <Helix/RenderPipelines/Core/Volume/Volume.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipeline.h>

namespace helix::rendering::universal
{
    UniversalAdditionalCameraData::~UniversalAdditionalCameraData() = default;

    bool UniversalAdditionalCameraData::requiresDepthTexture() const
    {
        if (requiresDepthOption_ != CameraOverrideOption::UsePipelineSettings)
            return requiresDepthOption_ == CameraOverrideOption::On;
        UniversalRenderPipelineAsset* asset = UniversalRenderPipeline::asset();
        return asset && asset->supportsCameraDepthTexture();
    }

    bool UniversalAdditionalCameraData::requiresColorTexture() const
    {
        if (requiresColorOption_ != CameraOverrideOption::UsePipelineSettings)
            return requiresColorOption_ == CameraOverrideOption::On;
        UniversalRenderPipelineAsset* asset = UniversalRenderPipeline::asset();
        return asset && asset->supportsCameraOpaqueTexture();
    }

    ScriptableRenderer* UniversalAdditionalCameraData::scriptableRenderer() const
    {
        UniversalRenderPipelineAsset* asset = UniversalRenderPipeline::asset();
        return asset ? asset->GetRenderer(rendererIndex_) : nullptr;
    }

    bool UniversalAdditionalCameraData::requiresVolumeFrameworkUpdate() const
    {
        if (volumeUpdateMode_ != VolumeFrameworkUpdateMode::UsePipelineSettings)
            return volumeUpdateMode_ != VolumeFrameworkUpdateMode::ViaScripting;
        UniversalRenderPipelineAsset* asset = UniversalRenderPipeline::asset();
        return !asset || asset->volumeFrameworkUpdateMode() != VolumeFrameworkUpdateMode::ViaScripting;
    }

    void UniversalAdditionalCameraData::OnDestroy()
    {
        if (VolumeStack* stack = volumeStack())
            VolumeManager::instance().DestroyStack(stack);
        volumeStack_ = 0;
    }

    UniversalAdditionalCameraData* GetUniversalAdditionalCameraData(engine::Camera* camera)
    {
        if (!camera || !camera->gameObject())
            return nullptr;
        engine::GameObject* go = camera->gameObject();
        scripting::TypeId type = scripting::TypeOf<UniversalAdditionalCameraData>();
        if (engine::Component* c = go->GetComponent(type))
            return static_cast<UniversalAdditionalCameraData*>(c);
        return static_cast<UniversalAdditionalCameraData*>(go->AddComponent(type));
    }

    UniversalAdditionalLightData* GetUniversalAdditionalLightData(engine::Light* light)
    {
        if (!light || !light->gameObject())
            return nullptr;
        engine::GameObject* go = light->gameObject();
        scripting::TypeId type = scripting::TypeOf<UniversalAdditionalLightData>();
        if (engine::Component* c = go->GetComponent(type))
            return static_cast<UniversalAdditionalLightData*>(c);
        return static_cast<UniversalAdditionalLightData*>(go->AddComponent(type));
    }
}
