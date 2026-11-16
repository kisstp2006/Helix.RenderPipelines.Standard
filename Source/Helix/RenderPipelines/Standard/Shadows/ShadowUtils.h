#pragma once

#include <Helix/Engine/Rendering/Pipeline/RenderStates.h>
#include <Helix/Engine/Rendering/Pipeline/Shadows.h>
#include <Helix/Engine/Rendering/Pipeline/CommandBuffer.h>
#include <Helix/Math/Matrix4x4.h>

// The shadow math of the pipeline (the reference's ShadowUtils; its C# class mirrors these): a cascade's view,
// projection and atlas tile, the tile's transform, the tile size of an atlas, a light's depth and normal bias.
namespace helix::rendering::universal
{
    class UniversalShadowData;

    // A shadow map tile of a light or cascade.
    struct ShadowSliceData
    {
        engine::ShadowSplitData splitData;
        i32 offsetX = 0, offsetY = 0;
        i32 resolution = 0;
        Matrix4x4 viewMatrix = Matrix4x4::identity;
        Matrix4x4 projectionMatrix = Matrix4x4::identity;
        // World to the atlas's texels (xy 0 to 1) and depth (0 to 1).
        Matrix4x4 shadowTransform = Matrix4x4::identity;

        void Clear() { *this = ShadowSliceData(); }
    };

    struct ShadowUtils
    {
        // The cascade's view and projection, its sphere (cascadeSplitDistance) and its tile in the atlas (side by
        // side, then the next row); false when the light casts no shadow there.
        static bool ExtractDirectionalLightMatrix(const engine::CullingResults& cullResults, const UniversalShadowData& shadowData, i32 shadowLightIndex,
                                                  i32 cascadeIndex, i32 shadowmapWidth, i32 shadowmapHeight, i32 shadowResolution, f32 shadowNearPlane,
                                                  Vector4& cascadeSplitDistance, ShadowSliceData& shadowSliceData);
        // The tile's place in the atlas applied to its transform.
        static void ApplySliceTransform(ShadowSliceData& shadowSliceData, i32 atlasWidth, i32 atlasHeight);
        // The largest square tile of which the atlas holds tileCount.
        static i32 GetMaxTileResolutionInAtlas(i32 atlasWidth, i32 atlasHeight, i32 tileCount);
        // The light's depth bias (x) and normal bias (y) in world units: the asset's or the light's values times the
        // size of one texel of the tile (2.5 times that with soft shadows); negative, as the caster passes apply them.
        static Vector4 GetShadowBias(const engine::VisibleLight& shadowLight, i32 shadowLightIndex, const UniversalShadowData& shadowData,
                                     const Matrix4x4& lightProjectionMatrix, f32 shadowResolution);
        // The fade of shadows over the last part of a distance (squared distances, as the shaders compare them):
        // fade = saturate(d^2 * scale + bias).
        static void GetScaleAndBiasForLinearDistanceFade(f32 fadeDistance, f32 border, f32& scale, f32& bias);
        // World to the atlas's texels for a light's projection and view (the GPU projection of a texture target).
        static Matrix4x4 GetShadowTransform(const Matrix4x4& projection, const Matrix4x4& view);
        // The soft shadow quality the shaders read (0 hard, 1 low, 2 medium, 3 high): the light's own, else the
        // asset's; 0 when soft shadows are off.
        static f32 SoftShadowQualityToShaderProperty(engine::Light* light, bool softShadowsEnabled);
        // What the caster passes read while a light's casters draw: _ShadowBias, _LightDirection, _LightPosition.
        static void SetupShadowCasterConstantBuffer(engine::CommandBuffer& cmd, const engine::VisibleLight& shadowLight, const Vector4& shadowBias);
    };
}
