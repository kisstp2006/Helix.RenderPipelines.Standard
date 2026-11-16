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
    Matrix4x4 ShadowUtils::GetShadowTransform(const Matrix4x4& projection, const Matrix4x4& view)
    {
        // The engine's GPU projection has its depth in 0 to 1 already: only xy go from -1..1 to 0..1.
        Matrix4x4 gpu = engine::GL::GetGPUProjectionMatrix(projection, false);
        Matrix4x4 scaleBias = Matrix4x4::identity;
        scaleBias(0, 0) = 0.5f;
        scaleBias(1, 1) = 0.5f;
        scaleBias(0, 3) = 0.5f;
        scaleBias(1, 3) = 0.5f;
        return scaleBias * gpu * view;
    }

    bool ShadowUtils::ExtractDirectionalLightMatrix(const engine::CullingResults& cullResults, const UniversalShadowData& shadowData, i32 shadowLightIndex,
                                                    i32 cascadeIndex, i32 shadowmapWidth, i32 shadowmapHeight, i32 shadowResolution, f32 shadowNearPlane,
                                                    Vector4& cascadeSplitDistance, ShadowSliceData& slice)
    {
        engine::CullingResults culling = cullResults;
        bool ok = culling.ComputeDirectionalShadowMatricesAndCullingPrimitives(shadowLightIndex, cascadeIndex, Max(1, shadowData.mainLightShadowCascadesCount),
                                                                                shadowData.mainLightShadowCascadesSplit, shadowResolution, shadowNearPlane,
                                                                                slice.viewMatrix, slice.projectionMatrix, slice.splitData);
        cascadeSplitDistance = slice.splitData.cullingSphere;
        slice.splitData.shadowCascadeBlendCullingFactor = 1.0f;
        slice.offsetX = (cascadeIndex % 2) * shadowResolution;
        slice.offsetY = (cascadeIndex / 2) * shadowResolution;
        slice.resolution = shadowResolution;
        slice.shadowTransform = GetShadowTransform(slice.projectionMatrix, slice.viewMatrix);
        // Cascades share the atlas: each transform goes to its tile.
        if (shadowData.mainLightShadowCascadesCount > 1)
            ApplySliceTransform(slice, shadowmapWidth, shadowmapHeight);
        return ok;
    }

    void ShadowUtils::ApplySliceTransform(ShadowSliceData& slice, i32 atlasWidth, i32 atlasHeight)
    {
        Matrix4x4 sliceTransform = Matrix4x4::identity;
        f32 oneOverAtlasWidth = 1.0f / f32(atlasWidth), oneOverAtlasHeight = 1.0f / f32(atlasHeight);
        sliceTransform(0, 0) = f32(slice.resolution) * oneOverAtlasWidth;
        sliceTransform(1, 1) = f32(slice.resolution) * oneOverAtlasHeight;
        sliceTransform(0, 3) = f32(slice.offsetX) * oneOverAtlasWidth;
        sliceTransform(1, 3) = f32(slice.offsetY) * oneOverAtlasHeight;
        slice.shadowTransform = sliceTransform * slice.shadowTransform;
    }

    i32 ShadowUtils::GetMaxTileResolutionInAtlas(i32 atlasWidth, i32 atlasHeight, i32 tileCount)
    {
        i32 resolution = Min(atlasWidth, atlasHeight);
        if (resolution <= 0 || tileCount <= 0)
            return 0;
        i32 count = atlasWidth / resolution * atlasHeight / resolution;
        while (count < tileCount)
        {
            resolution >>= 1;
            if (resolution == 0)
                return 0;
            count = atlasWidth / resolution * atlasHeight / resolution;
        }
        return resolution;
    }

    Vector4 ShadowUtils::GetShadowBias(const engine::VisibleLight& shadowLight, i32 shadowLightIndex, const UniversalShadowData& shadowData,
                                       const Matrix4x4& lightProjectionMatrix, f32 shadowResolution)
    {
        if (shadowLightIndex < 0 || u32(shadowLightIndex) >= shadowData.bias.Size() || shadowResolution <= 0)
            return Vector4();
        f32 frustumSize = 0;
        switch (shadowLight.lightType)
        {
        case engine::LightType::Directional: frustumSize = 2.0f / lightProjectionMatrix(0, 0); break;
        case engine::LightType::Spot: frustumSize = std::tan(shadowLight.spotAngle * 0.5f * mathf::Deg2Rad) * shadowLight.range; break;
        case engine::LightType::Point: frustumSize = std::tan(90.0f * 0.5f * mathf::Deg2Rad) * shadowLight.range; break;
        default: break;
        }
        f32 texelSize = frustumSize / shadowResolution;
        f32 depthBias = -shadowData.bias[u32(shadowLightIndex)].x * texelSize;
        f32 normalBias = -shadowData.bias[u32(shadowLightIndex)].y * texelSize;
        // A soft shadow samples up to a few texels away: the bias scaled by the filter's radius.
        engine::Light* light = shadowLight.light();
        if (shadowData.supportsSoftShadows && light && light->shadows() == engine::LightShadows::Soft)
        {
            constexpr f32 kernelRadius = 2.5f;
            depthBias *= kernelRadius;
            normalBias *= kernelRadius;
        }
        return Vector4(depthBias, normalBias, 0, 0);
    }

    void ShadowUtils::GetScaleAndBiasForLinearDistanceFade(f32 fadeDistance, f32 border, f32& scale, f32& bias)
    {
        if (border < 0.0001f)
        {
            constexpr f32 multiplier = 1000.0f;
            scale = multiplier;
            bias = -fadeDistance * multiplier;
            return;
        }
        border = 1.0f - border;
        border *= border;
        // A linear fade from (1 - border)^2 of the distance to the distance itself.
        f32 distanceFadeNear = border * fadeDistance;
        scale = 1.0f / (fadeDistance - distanceFadeNear);
        bias = -distanceFadeNear / (fadeDistance - distanceFadeNear);
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
        static const i32 biasId = engine::Shader::PropertyToID("_ShadowBias");
        static const i32 directionId = engine::Shader::PropertyToID("_LightDirection");
        static const i32 positionId = engine::Shader::PropertyToID("_LightPosition");
        cmd.SetGlobalVector(biasId, shadowBias);
        // Toward the light: the negated forward axis of its transform.
        Vector4 forward = shadowLight.localToWorldMatrix.GetColumn(2);
        cmd.SetGlobalVector(directionId, Vector4(-forward.x, -forward.y, -forward.z, 0));
        Vector4 position = shadowLight.localToWorldMatrix.GetColumn(3);
        cmd.SetGlobalVector(positionId, Vector4(position.x, position.y, position.z, 1));
    }
}
