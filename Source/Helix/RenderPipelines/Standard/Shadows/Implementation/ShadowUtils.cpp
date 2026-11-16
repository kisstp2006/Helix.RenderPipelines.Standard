#include <Helix/RenderPipelines/Standard/Shadows/ShadowUtils.h>

#include <Helix/Engine/Rendering/GL.h>
#include <Helix/Engine/Rendering/Light.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/RenderPipelines/Standard/Components/AdditionalData.h>
#include <Helix/RenderPipelines/Standard/Frame/FrameData.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipeline.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipelineAsset.h>

#include <cmath>

namespace helix::rendering::universal
{
    Matrix4x4 ShadowUtils::GetShadowTransform(const Matrix4x4& projection, const Matrix4x4& view) { return CascadedShadows::GetShadowTransform(projection, view); }

    bool ShadowUtils::ExtractDirectionalLightMatrix(const engine::CullingResults& cullResults, const UniversalShadowData& shadowData, i32 shadowLightIndex,
                                                    i32 cascadeIndex, i32 shadowmapWidth, i32 shadowmapHeight, i32 shadowResolution, f32 shadowNearPlane,
                                                    Vector4& cascadeSplitDistance, ShadowSliceData& slice)
    {
        return CascadedShadows::ExtractDirectionalLightMatrix(cullResults, shadowLightIndex, cascadeIndex, shadowData.mainLightShadowCascadesCount,
                                                              shadowData.mainLightShadowCascadesSplit, shadowmapWidth, shadowmapHeight, shadowResolution,
                                                              shadowNearPlane, cascadeSplitDistance, slice);
    }

    void ShadowUtils::ApplySliceTransform(ShadowSliceData& slice, i32 atlasWidth, i32 atlasHeight) { CascadedShadows::ApplySliceTransform(slice, atlasWidth, atlasHeight); }

    i32 ShadowUtils::GetMaxTileResolutionInAtlas(i32 atlasWidth, i32 atlasHeight, i32 tileCount)
    {
        return CascadedShadows::GetMaxTileResolutionInAtlas(atlasWidth, atlasHeight, tileCount);
    }

    Vector4 ShadowUtils::GetShadowBias(const engine::VisibleLight& shadowLight, i32 shadowLightIndex, const UniversalShadowData& shadowData,
                                       const Matrix4x4& lightProjectionMatrix, f32 shadowResolution)
    {
        if (shadowLightIndex < 0 || u32(shadowLightIndex) >= shadowData.bias.Size() || shadowResolution <= 0)
            return Vector4();
        engine::Light* light = shadowLight.light();
        const Vector4& bias = shadowData.bias[u32(shadowLightIndex)];
        bool soft = shadowData.supportsSoftShadows && light && light->shadows() == engine::LightShadows::Soft;
        return CascadedShadows::GetShadowBias(shadowLight, Vector2(bias.x, bias.y), soft, lightProjectionMatrix, shadowResolution);
    }

    void ShadowUtils::GetScaleAndBiasForLinearDistanceFade(f32 fadeDistance, f32 border, f32& scale, f32& bias)
    {
        CascadedShadows::GetScaleAndBiasForLinearDistanceFade(fadeDistance, border, scale, bias);
    }

    f32 ShadowUtils::SoftShadowQualityToShaderProperty(engine::Light* light, bool softShadowsEnabled)
    {
        if (!softShadowsEnabled)
            return 0.0f;
        SoftShadowQuality quality = SoftShadowQuality::UsePipelineSettings;
        if (UniversalAdditionalLightData* extra = GetUniversalAdditionalLightData(light))
            quality = extra->softShadowQuality();
        if (quality == SoftShadowQuality::UsePipelineSettings)
        {
            UniversalRenderPipelineAsset* asset = UniversalRenderPipeline::asset();
            quality = asset ? asset->softShadowQuality() : SoftShadowQuality::Medium;
        }
        return f32(Max(i32(quality), i32(SoftShadowQuality::Low)));
    }

    void ShadowUtils::SetupShadowCasterConstantBuffer(engine::CommandBuffer& cmd, const engine::VisibleLight& shadowLight, const Vector4& shadowBias)
    {
        CascadedShadows::SetupShadowCasterConstantBuffer(cmd, shadowLight, shadowBias);
    }
}
