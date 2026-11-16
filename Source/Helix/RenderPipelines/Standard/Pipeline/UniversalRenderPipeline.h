#pragma once

#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipelineAsset.h>
#include <Helix/RenderPipelines/Core/PostProcessing/PostProcessing.h>
#include <Helix/RenderPipelines/Core/RenderGraph/RenderGraph.h>

// The Standard pipeline: what the engine renders the cameras with while a UniversalRenderPipelineAsset is set.
namespace helix::rendering::universal
{
    class UniversalRenderPipeline final : public engine::RenderPipeline
    {
    public:
        // The value of the RenderPipeline tag of the SubShaders it draws.
        static constexpr StringView k_ShaderTagName = "UniversalPipeline";
        // The most lights per object, and the limits of the asset's settings.
        static constexpr i32 maxPerObjectLights = 8;
        static constexpr f32 minRenderScale = 0.1f;
        static constexpr f32 maxRenderScale = 2.0f;
        static constexpr f32 maxShadowBias = 10.0f;
        static constexpr i32 maxNumIterationsEnclosingSphere = 1000;
        // The most additional lights of a camera: 32 on OpenGL ES and WebGL 2, else 256.
        static i32 maxVisibleAdditionalLights();
        // The pipeline asset in use when it is this pipeline's (else null).
        static UniversalRenderPipelineAsset* asset();

        explicit UniversalRenderPipeline(UniversalRenderPipelineAsset* asset);
        ~UniversalRenderPipeline() override;

        void Render(engine::ScriptableRenderContext& context, Span<engine::Camera* const> cameras) override;

        UniversalRenderPipelineAsset* pipelineAsset() const { return asset_.Get(); }

        // A game camera (not the scene view's or a preview's).
        static bool IsGameCamera(engine::Camera* camera) { return camera && camera->cameraType() == engine::CameraType::Game; }
        // A light's attenuation (x: 1 over its squared range, y: the smooth fade near the range, z and w: the spot's
        // angle falloff) and the spot's direction (toward the light), as the shaders read them.
        static void GetLightAttenuationAndSpotDirection(engine::LightType lightType, f32 lightRange, const Matrix4x4& lightLocalToWorldMatrix, f32 spotAngle,
                                                        const f32* innerSpotAngle, Vector4& lightAttenuation, Vector4& lightSpotDir);
        // A visible light's constants: its position (w 0: a direction toward the light), color, attenuation, spot
        // direction and occlusion probe channel.
        static void InitializeLightConstants_Common(Span<const engine::VisibleLight> lights, i32 lightIndex, Vector4& lightPos, Vector4& lightColor,
                                                    Vector4& lightAttenuation, Vector4& lightSpotDir, Vector4& lightOcclusionProbeChannel);

    protected:
        void Dispose(bool disposing) override;

    private:
        void RenderCamera(engine::ScriptableRenderContext& context, engine::Camera* camera);
        // The frame data of a camera from the asset, the camera and its additional data.
        void FillCameraData(engine::Camera* camera, ScriptableRenderer* renderer);
        // The main light (the brightest directional one), the additional lights' map and their constants.
        void SetupLights(engine::CommandBuffer& cmd, engine::CullingResults culling);

        engine::PPtr<UniversalRenderPipelineAsset> asset_;
        RenderGraph graph_;
        PostProcessPasses post_;
        ContextContainer frame_;
    };

    // What C#'s UniversalRenderPipeline calls (its constants and asset).
    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal", Static, Internal)
    class UniversalRenderPipelineNative
    {
    public:
        HX_FUNCTION() static i32 MaxVisibleAdditionalLights() { return UniversalRenderPipeline::maxVisibleAdditionalLights(); }
        HX_FUNCTION() static UniversalRenderPipelineAsset* Asset() { return UniversalRenderPipeline::asset(); }
        HX_FUNCTION() static bool IsGameCamera(engine::Camera* camera) { return UniversalRenderPipeline::IsGameCamera(camera); }
        HX_FUNCTION(Out = "lightAttenuation,lightSpotDir") static void LightAttenuation(engine::LightType lightType, f32 lightRange, Matrix4x4 lightLocalToWorldMatrix,
                                                                                    f32 spotAngle, bool hasInner, f32 innerSpotAngle,
                                                                                    Vector4& lightAttenuation, Vector4& lightSpotDir)
        {
            UniversalRenderPipeline::GetLightAttenuationAndSpotDirection(lightType, lightRange, lightLocalToWorldMatrix, spotAngle, hasInner ? &innerSpotAngle : nullptr,
                                                                         lightAttenuation, lightSpotDir);
        }
        HX_FUNCTION(Out = "lightPos,lightColor,lightAttenuation,lightSpotDir,lightOcclusionProbeChannel") static void LightConstants(
            u64 lights, i32 count, i32 lightIndex, Vector4& lightPos, Vector4& lightColor, Vector4& lightAttenuation, Vector4& lightSpotDir,
            Vector4& lightOcclusionProbeChannel)
        {
            UniversalRenderPipeline::InitializeLightConstants_Common(
                Span<const engine::VisibleLight>(reinterpret_cast<const engine::VisibleLight*>(uintptr_t(lights)), usize(count > 0 ? count : 0)), lightIndex, lightPos,
                lightColor, lightAttenuation, lightSpotDir, lightOcclusionProbeChannel);
        }
    };
}
