#pragma once

#include <Helix/Engine/Material/Material.h>
#include <Helix/RenderPipelines/Core/Context/ContextContainer.h>
#include <Helix/RenderPipelines/Core/RenderGraph/RenderGraph.h>
#include <Helix/RenderPipelines/Core/Volume/VolumeComponent.h>

// The forward renderer's post processing (the reference's ColorGradingLutPass and PostProcessPass): the color grading
// look up table of the frame, bloom's chain and the uber pass that puts bloom, exposure and the table on the camera's
// color.
namespace helix::rendering::universal
{
    class UniversalCameraData;
    class UniversalPostProcessingData;
    class UniversalRenderPipelineAsset;
    class PostProcessData;

    // The format of HDR colors: B10G11R11 for 32 bit precision where the device renders into it, else R16G16B16A16.
    engine::GraphicsFormat HdrColorFormat(const UniversalRenderPipelineAsset* asset);

    class ColorGradingLutPass
    {
    public:
        ~ColorGradingLutPass();

        // The table (lutSize^2 x lutSize) with the stack's ColorAdjustments and, for HDR grading, Tonemapping baked in.
        TextureHandle Record(RenderGraph& graph, const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData, const VolumeStack* stack,
                             const PostProcessData& resources);
        // Whether the frame grades in HDR (the asset's mode and an HDR camera).
        static bool HdrGrading(const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData);

    private:
        engine::PPtr<engine::Material> ldr_, hdr_;
    };

    class PostProcessPass
    {
    public:
        ~PostProcessPass();

        // Bloom's chain of the source (when the stack's Bloom is active), then the uber pass into a new texture of the
        // source's description: the result.
        TextureHandle Record(RenderGraph& graph, const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData,
                             const VolumeStack* stack, const PostProcessData& resources, TextureHandle source, TextureHandle lut);

    private:
        TextureHandle RecordBloom(RenderGraph& graph, const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData,
                                  const VolumeStack* stack, const PostProcessData& resources, TextureHandle source);

        engine::PPtr<engine::Material> bloom_, uber_;
    };
}
