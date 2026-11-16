using HelixEngine.Rendering;

namespace HelixEngine.Rendering.Universal
{
    /// <summary>A shadow map tile of a light or cascade: its culling, its place and size in the atlas, the light's view
    /// and projection and the transform from world space to the atlas.</summary>
    public struct ShadowSliceData
    {
        public ShadowSplitData splitData;
        public int offsetX;
        public int offsetY;
        public int resolution;
        public Matrix4x4 viewMatrix;
        public Matrix4x4 projectionMatrix;
        public Matrix4x4 shadowTransform;

        public void Clear()
        {
            splitData = default;
            offsetX = offsetY = resolution = 0;
            viewMatrix = projectionMatrix = shadowTransform = Matrix4x4.identity;
        }
    }

    /// <summary>The shadow math of the pipeline, the same as its native side: a cascade's matrices and tile, the tile's
    /// transform, the tile size of an atlas, a light's depth and normal bias.</summary>
    public static class ShadowUtils
    {
        /// <summary>The cascade's view and projection, its sphere and its tile in the atlas (side by side, then the next
        /// row); false when the light casts no shadow there.</summary>
        public static bool ExtractDirectionalLightMatrix(ref CullingResults cullResults, UniversalShadowData shadowData, int shadowLightIndex, int cascadeIndex,
                                                         int shadowmapWidth, int shadowmapHeight, int shadowResolution, float shadowNearPlane,
                                                         out Vector4 cascadeSplitDistance, out ShadowSliceData shadowSliceData)
        {
            shadowSliceData = default;
            bool success = cullResults.ComputeDirectionalShadowMatricesAndCullingPrimitives(shadowLightIndex, cascadeIndex,
                                                                                           Mathf.Max(1, shadowData.mainLightShadowCascadesCount),
                                                                                           shadowData.mainLightShadowCascadesSplit, shadowResolution, shadowNearPlane,
                                                                                           out shadowSliceData.viewMatrix, out shadowSliceData.projectionMatrix,
                                                                                           out shadowSliceData.splitData);
            cascadeSplitDistance = shadowSliceData.splitData.cullingSphere;
            shadowSliceData.splitData.shadowCascadeBlendCullingFactor = 1.0f;
            shadowSliceData.offsetX = (cascadeIndex % 2) * shadowResolution;
            shadowSliceData.offsetY = (cascadeIndex / 2) * shadowResolution;
            shadowSliceData.resolution = shadowResolution;
            shadowSliceData.shadowTransform = GetShadowTransform(shadowSliceData.projectionMatrix, shadowSliceData.viewMatrix);
            // Cascades share the atlas: each transform goes to its tile.
            if (shadowData.mainLightShadowCascadesCount > 1)
                ApplySliceTransform(ref shadowSliceData, shadowmapWidth, shadowmapHeight);
            return success;
        }

        /// <summary>The tile's place in the atlas applied to its transform.</summary>
        public static void ApplySliceTransform(ref ShadowSliceData shadowSliceData, int atlasWidth, int atlasHeight)
        {
            Matrix4x4 sliceTransform = Matrix4x4.identity;
            float oneOverAtlasWidth = 1.0f / atlasWidth;
            float oneOverAtlasHeight = 1.0f / atlasHeight;
            sliceTransform.m00 = shadowSliceData.resolution * oneOverAtlasWidth;
            sliceTransform.m11 = shadowSliceData.resolution * oneOverAtlasHeight;
            sliceTransform.m03 = shadowSliceData.offsetX * oneOverAtlasWidth;
            sliceTransform.m13 = shadowSliceData.offsetY * oneOverAtlasHeight;
            shadowSliceData.shadowTransform = sliceTransform * shadowSliceData.shadowTransform;
        }

        /// <summary>The largest square tile of which the atlas holds tileCount.</summary>
        public static int GetMaxTileResolutionInAtlas(int atlasWidth, int atlasHeight, int tileCount)
        {
            int resolution = Mathf.Min(atlasWidth, atlasHeight);
            if (resolution <= 0 || tileCount <= 0)
                return 0;
            int currentTileCount = atlasWidth / resolution * atlasHeight / resolution;
            while (currentTileCount < tileCount)
            {
                resolution >>= 1;
                if (resolution == 0)
                    return 0;
                currentTileCount = atlasWidth / resolution * atlasHeight / resolution;
            }
            return resolution;
        }

        /// <summary>The light's depth bias (x) and normal bias (y) in world units: the asset's or the light's values
        /// times the size of one texel of the tile (2.5 times that with soft shadows); negative, as the caster passes
        /// apply them.</summary>
        public static Vector4 GetShadowBias(ref VisibleLight shadowLight, int shadowLightIndex, UniversalShadowData shadowData, Matrix4x4 lightProjectionMatrix,
                                            float shadowResolution)
        {
            Vector4[] bias = shadowData.biasArray;
            if (shadowLightIndex < 0 || bias == null || shadowLightIndex >= bias.Length || shadowResolution <= 0)
            {
                Debug.LogWarning($"{shadowLightIndex} is not a valid light index.");
                return Vector4.zero;
            }
            float frustumSize;
            if (shadowLight.lightType == LightType.Directional)
                frustumSize = 2.0f / lightProjectionMatrix.m00;
            else if (shadowLight.lightType == LightType.Spot)
                frustumSize = Mathf.Tan(shadowLight.spotAngle * 0.5f * Mathf.Deg2Rad) * shadowLight.range;
            else if (shadowLight.lightType == LightType.Point)
                frustumSize = Mathf.Tan(90.0f * 0.5f * Mathf.Deg2Rad) * shadowLight.range;
            else
            {
                Debug.LogWarning("Only point, spot and directional shadow casters are supported in the universal pipeline.");
                frustumSize = 0.0f;
            }
            float texelSize = frustumSize / shadowResolution;
            float depthBias = -bias[shadowLightIndex].x * texelSize;
            float normalBias = -bias[shadowLightIndex].y * texelSize;
            // A soft shadow samples up to a few texels away: the bias scaled by the filter's radius.
            Light light = shadowLight.light;
            if (shadowData.supportsSoftShadows && light != null && light.shadows == LightShadows.Soft)
            {
                const float kernelRadius = 2.5f;
                depthBias *= kernelRadius;
                normalBias *= kernelRadius;
            }
            return new Vector4(depthBias, normalBias, 0.0f, 0.0f);
        }

        /// <summary>The fade of shadows over the last part of a distance (squared distances, as the shaders compare
        /// them): fade = saturate(d^2 * scale + bias).</summary>
        internal static void GetScaleAndBiasForLinearDistanceFade(float fadeDistance, float border, out float scale, out float bias)
        {
            if (border < 0.0001f)
            {
                const float multiplier = 1000.0f;
                scale = multiplier;
                bias = -fadeDistance * multiplier;
                return;
            }
            border = 1.0f - border;
            border *= border;
            float distanceFadeNear = border * fadeDistance;
            scale = 1.0f / (fadeDistance - distanceFadeNear);
            bias = -distanceFadeNear / (fadeDistance - distanceFadeNear);
        }

        // World to the atlas's texels for a light's projection and view (the engine's GPU projection has its depth in 0
        // to 1 already: only xy go from -1..1 to 0..1).
        static Matrix4x4 GetShadowTransform(Matrix4x4 proj, Matrix4x4 view)
        {
            Matrix4x4 gpu = GL.GetGPUProjectionMatrix(proj, false);
            Matrix4x4 textureScaleAndBias = Matrix4x4.identity;
            textureScaleAndBias.m00 = 0.5f;
            textureScaleAndBias.m11 = 0.5f;
            textureScaleAndBias.m03 = 0.5f;
            textureScaleAndBias.m13 = 0.5f;
            return textureScaleAndBias * gpu * view;
        }
    }
}
