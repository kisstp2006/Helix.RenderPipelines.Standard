using HelixEngine.Builtin;
using HelixEngine.Rendering.ShaderLibrary;
using HelixEngine.Shaders;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>The shadow caster passes' bias (the core's ShadowCaster).</summary>
public static class UniversalShadowCaster
{
    public static float3 ApplyShadowBias(float3 positionWS, float3 normalWS, float3 lightDirection) => ShadowCaster.ApplyShadowBias(positionWS, normalWS, lightDirection);

    public static float4 ShadowPositionCS(float3 positionOS, float3 normalOS) => ShadowCaster.ShadowPositionCS(positionOS, normalOS);
}

/// <summary>The main light's cascaded shadow (the reference's Shadows): the core's CascadedShadows.</summary>
public static class UniversalShadows
{
    public static int ComputeCascadeIndex(float3 positionWS) => CascadedShadows.ComputeCascadeIndex(positionWS);

    public static float4 TransformWorldToShadowCoord(float3 positionWS) => CascadedShadows.TransformWorldToShadowCoord(positionWS);

    public static float SampleShadowmap(float3 coord, float quality) => CascadedShadows.SampleShadowmap(coord, quality);

    public static float GetMainLightShadowFade(float3 positionWS) => CascadedShadows.GetMainLightShadowFade(positionWS);

    public static float MainLightRealtimeShadow(float3 positionWS) => CascadedShadows.MainLightRealtimeShadow(positionWS);
}
