using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>What a shadow caster pass reads while the pipeline draws a light's casters.</summary>
public struct ShadowCasterConstants
{
    /// <summary>x: the depth bias along the light (negative: away from it); y: the normal bias (negative: inward).</summary>
    public float4 _ShadowBias;
    /// <summary>xyz: the direction toward the light (directional lights).</summary>
    public float4 _LightDirection;
    /// <summary>xyz: the light's position (spot and point lights).</summary>
    public float4 _LightPosition;
}

/// <summary>The globals of shadow caster passes (uniform slot 1; casters read no other pipeline globals).</summary>
public static class UniversalShadowCaster
{
    [Uniform(1)] public static ShadowCasterConstants caster;

    /// <summary>A caster's position moved toward the light by the depth bias and inward along its normal by the normal
    /// bias (more where the surface turns from the light).</summary>
    public static float3 ApplyShadowBias(float3 positionWS, float3 normalWS, float3 lightDirection)
    {
        float invNdotL = 1.0f - Saturate(Dot(lightDirection, normalWS));
        float scale = invNdotL * caster._ShadowBias.y;
        positionWS = lightDirection * caster._ShadowBias.x + positionWS;
        return normalWS * scale + positionWS;
    }

    /// <summary>A caster's clip position for the light's view: biased, its depth clamped to the near plane (casters in
    /// front of the cascade still cast).</summary>
    public static float4 ShadowPositionCS(float3 positionOS, float3 normalOS)
    {
        float3 positionWS = ShaderVariables.ObjectToWorld(positionOS);
        float3 normalWS = ShaderVariables.ObjectToWorldNormal(normalOS);
        float3 biased = ApplyShadowBias(positionWS, normalWS, caster._LightDirection.xyz);
        float4 positionCS = Mul(ShaderVariables.camera.helix_MatrixVP, new float4(biased, 1.0f));
        positionCS.z = Max(positionCS.z, 0.0f);
        return positionCS;
    }
}

/// <summary>
/// The main light's cascaded shadow (the reference's Shadows): the cascade of a position by the culling spheres, its
/// coordinate in the atlas, the hard tap and the soft tiers (Low: four bilinear compared taps; Medium: the 5x5 tent in
/// nine taps; High: the 7x7 tent in sixteen), the strength and the fade at the shadow distance.
/// </summary>
public static class UniversalShadows
{
    /// <summary>The first cascade whose sphere holds the position (the cascade count beyond them).</summary>
    public static int ComputeCascadeIndex(float3 positionWS)
    {
        MainLightShadowConstants s = UniversalGlobals.shadows;
        float3 f0 = positionWS - s._CascadeShadowSplitSpheres0.xyz;
        float3 f1 = positionWS - s._CascadeShadowSplitSpheres1.xyz;
        float3 f2 = positionWS - s._CascadeShadowSplitSpheres2.xyz;
        float3 f3 = positionWS - s._CascadeShadowSplitSpheres3.xyz;
        float4 distances2 = new float4(Dot(f0, f0), Dot(f1, f1), Dot(f2, f2), Dot(f3, f3));
        float4 radii = s._CascadeShadowSplitSphereRadii;
        float4 weights = new float4(distances2.x < radii.x ? 1.0f : 0.0f, distances2.y < radii.y ? 1.0f : 0.0f, distances2.z < radii.z ? 1.0f : 0.0f,
                                    distances2.w < radii.w ? 1.0f : 0.0f);
        weights.y = Saturate(weights.y - weights.x);
        weights.z = Saturate(weights.z - weights.y - weights.x);
        weights.w = Saturate(weights.w - weights.z - weights.y - weights.x);
        return (int)(4.0f - Dot(weights, new float4(4.0f, 3.0f, 2.0f, 1.0f)));
    }

    /// <summary>The position in the atlas (xy) at its depth (z), by its cascade; w: the cascade.</summary>
    public static float4 TransformWorldToShadowCoord(float3 positionWS)
    {
        int cascade = ComputeCascadeIndex(positionWS);
        float4 coord = Mul(UniversalGlobals.shadows._MainLightWorldToShadow[cascade], new float4(positionWS, 1.0f));
        return new float4(coord.xyz, (float)cascade);
    }

    static float Compare(float2 uv, float depth) =>
        UniversalGlobals._MainLightShadowmapTexture.SampleCmpLevelZero(UniversalGlobals.sampler_MainLightShadowmapTexture, uv, depth);

    // ------------------------------------------------------------------------------------------------ the tent filters

    static float TriangleTexelArea(float triangleHeight) => triangleHeight - 0.5f;

    // The areas of the texels under a 3 texel wide tent at an offset: cut by the texel edges, and whole.
    struct TentAreas
    {
        public float4 cut;
        public float4 uncut;
    }

    static TentAreas TexelAreasTent3x3(float offset)
    {
        float a = offset + 0.5f;
        float offsetSquaredHalved = a * a * 0.5f;
        TentAreas t;
        t.cut.x = offsetSquaredHalved - offset;
        t.cut.w = offsetSquaredHalved;
        t.uncut.x = t.cut.x;
        t.uncut.w = t.cut.w;
        t.uncut.y = TriangleTexelArea(1.5f - offset);
        float left = Min(offset, 0.0f);
        t.cut.y = t.uncut.y - left * left;
        t.uncut.z = TriangleTexelArea(1.5f + offset);
        float right = Max(offset, 0.0f);
        t.cut.z = t.uncut.z - right * right;
        return t;
    }

    static float Tent5x5(float4 texelSize, float3 coord)
    {
        float2 center = coord.xy * texelSize.zw;
        float2 fetchCenter = Floor(center + new float2(0.5f));
        float2 offset = center - fetchCenter;
        TentAreas tu = TexelAreasTent3x3(offset.x), tv = TexelAreasTent3x3(offset.y);
        float4 areaU = tu.cut, uncutU = tu.uncut, areaV = tv.cut, uncutV = tv.uncut;
        float3 aU = 0.16f * new float3(areaU.x, uncutU.y, areaU.y + 1.0f);
        float3 bU = 0.16f * new float3(areaU.z + 1.0f, uncutU.z, areaU.w);
        float3 aV = 0.16f * new float3(areaV.x, uncutV.y, areaV.y + 1.0f);
        float3 bV = 0.16f * new float3(areaV.z + 1.0f, uncutV.z, areaV.w);
        float3 weightsU = new float3(aU.x, aU.z, bU.y) + new float3(aU.y, bU.x, bU.z);
        float3 weightsV = new float3(aV.x, aV.z, bV.y) + new float3(aV.y, bV.x, bV.z);
        float3 offsetsU = (new float3(aU.y, bU.x, bU.z) / weightsU + new float3(-2.5f, -0.5f, 1.5f)) * texelSize.x;
        float3 offsetsV = (new float3(aV.y, bV.x, bV.z) / weightsV + new float3(-2.5f, -0.5f, 1.5f)) * texelSize.y;
        float2 origin = fetchCenter * texelSize.xy;
        float sum = 0.0f;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
            {
                float u = i == 0 ? offsetsU.x : i == 1 ? offsetsU.y : offsetsU.z;
                float v = j == 0 ? offsetsV.x : j == 1 ? offsetsV.y : offsetsV.z;
                float wu = i == 0 ? weightsU.x : i == 1 ? weightsU.y : weightsU.z;
                float wv = j == 0 ? weightsV.x : j == 1 ? weightsV.y : weightsV.z;
                sum += wu * wv * Compare(origin + new float2(u, v), coord.z);
            }
        return sum;
    }

    static float Tent7x7(float4 texelSize, float3 coord)
    {
        float2 center = coord.xy * texelSize.zw;
        float2 fetchCenter = Floor(center + new float2(0.5f));
        float2 offset = center - fetchCenter;
        TentAreas tu = TexelAreasTent3x3(offset.x), tv = TexelAreasTent3x3(offset.y);
        float4 areaU = tu.cut, uncutU = tu.uncut, areaV = tv.cut, uncutV = tv.uncut;
        const float k = 0.081632f;
        float4 aU = k * new float4(areaU.x, uncutU.y, uncutU.y + 1.0f, areaU.y + 2.0f);
        float4 bU = k * new float4(areaU.z + 2.0f, uncutU.z + 1.0f, uncutU.z, areaU.w);
        float4 aV = k * new float4(areaV.x, uncutV.y, uncutV.y + 1.0f, areaV.y + 2.0f);
        float4 bV = k * new float4(areaV.z + 2.0f, uncutV.z + 1.0f, uncutV.z, areaV.w);
        float4 weightsU = new float4(aU.x, aU.z, bU.x, bU.z) + new float4(aU.y, aU.w, bU.y, bU.w);
        float4 weightsV = new float4(aV.x, aV.z, bV.x, bV.z) + new float4(aV.y, aV.w, bV.y, bV.w);
        float4 offsetsU = (new float4(aU.y, aU.w, bU.y, bU.w) / weightsU + new float4(-3.5f, -1.5f, 0.5f, 2.5f)) * texelSize.x;
        float4 offsetsV = (new float4(aV.y, aV.w, bV.y, bV.w) / weightsV + new float4(-3.5f, -1.5f, 0.5f, 2.5f)) * texelSize.y;
        float2 origin = fetchCenter * texelSize.xy;
        float sum = 0.0f;
        for (int j = 0; j < 4; j++)
            for (int i = 0; i < 4; i++)
            {
                float u = i == 0 ? offsetsU.x : i == 1 ? offsetsU.y : i == 2 ? offsetsU.z : offsetsU.w;
                float v = j == 0 ? offsetsV.x : j == 1 ? offsetsV.y : j == 2 ? offsetsV.z : offsetsV.w;
                float wu = i == 0 ? weightsU.x : i == 1 ? weightsU.y : i == 2 ? weightsU.z : weightsU.w;
                float wv = j == 0 ? weightsV.x : j == 1 ? weightsV.y : j == 2 ? weightsV.z : weightsV.w;
                sum += wu * wv * Compare(origin + new float2(u, v), coord.z);
            }
        return sum;
    }

    /// <summary>The share of the atlas's depth the coordinate is not behind, by the soft shadow quality (0 hard, 1 low,
    /// 2 medium, 3 high).</summary>
    public static float SampleShadowmap(float3 coord, float quality)
    {
        MainLightShadowConstants s = UniversalGlobals.shadows;
        if (quality < 0.5f)
            return Compare(coord.xy, coord.z);
        if (quality < 1.5f)
        {
            float4 a = new float4(Compare(coord.xy + s._MainLightShadowOffset0.xy, coord.z), Compare(coord.xy + s._MainLightShadowOffset0.zw, coord.z),
                                  Compare(coord.xy + s._MainLightShadowOffset1.xy, coord.z), Compare(coord.xy + s._MainLightShadowOffset1.zw, coord.z));
            return Dot(a, new float4(0.25f));
        }
        if (quality < 2.5f)
            return Tent5x5(s._MainLightShadowmapSize, coord);
        return Tent7x7(s._MainLightShadowmapSize, coord);
    }

    /// <summary>How much of the shadow fades at the position: none near the camera, all at the shadow distance.</summary>
    public static float GetMainLightShadowFade(float3 positionWS)
    {
        float3 cameraToPixel = positionWS - ShaderVariables.camera.helix_WorldSpaceCameraPos.xyz;
        float distance2 = Dot(cameraToPixel, cameraToPixel);
        MainLightShadowConstants s = UniversalGlobals.shadows;
        return Saturate(distance2 * s._MainLightShadowParams.z + s._MainLightShadowParams.w);
    }

    /// <summary>The main light's shadow at a position: 1 lit, 0 in shadow, by the light's strength and the fade.</summary>
    public static float MainLightRealtimeShadow(float3 positionWS)
    {
        MainLightShadowConstants s = UniversalGlobals.shadows;
        float strength = s._MainLightShadowParams.x;
        if (strength <= 0.0f)
            return 1.0f;
        float4 coord = TransformWorldToShadowCoord(positionWS);
        // Beyond the last cascade (its matrix is zero) or outside the light's depth.
        if (coord.z <= 0.0f || coord.z >= 1.0f)
            return 1.0f;
        float attenuation = SampleShadowmap(coord.xyz, s._MainLightShadowParams.y);
        attenuation = Lerp(1.0f, attenuation, strength);
        return Lerp(attenuation, 1.0f, GetMainLightShadowFade(positionWS));
    }
}
