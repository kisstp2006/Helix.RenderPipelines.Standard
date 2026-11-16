using HelixEngine.Builtin;
using HelixEngine.Shaders;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>The main light and the counts the pipeline sets each camera (globals by name).</summary>
public struct MainLightConstants
{
    /// <summary>xyz: the direction toward the light; w 0.</summary>
    public float4 _MainLightPosition;
    public float4 _MainLightColor;
    /// <summary>x: the most additional lights per object.</summary>
    public float4 _AdditionalLightsCount;
    /// <summary>The ambient probe's first coefficients times the reflection intensity: the reflection without environment reflections.</summary>
    public float4 _GlossyEnvironmentColor;
}

[System.Runtime.CompilerServices.InlineArray(UniversalGlobals.MaxVisibleAdditionalLights)]
public struct LightVectorArray
{
    private float4 element;
}

/// <summary>The additional lights' positions (w 1, or 0 with the direction toward a directional light).</summary>
public struct AdditionalLightPositions
{
    public LightVectorArray _AdditionalLightsPosition;
}

public struct AdditionalLightColors
{
    public LightVectorArray _AdditionalLightsColor;
}

/// <summary>x: 1 over the squared range; zw: the spot angle's scale and offset.</summary>
public struct AdditionalLightAttenuations
{
    public LightVectorArray _AdditionalLightsAttenuation;
}

/// <summary>The spot lights' directions (toward the light).</summary>
public struct AdditionalLightSpotDirections
{
    public LightVectorArray _AdditionalLightsSpotDir;
}

/// <summary>The ambient probe in the shaders' form (RenderSettings.ambientProbe).</summary>
public struct AmbientProbeConstants
{
    public float4 unity_SHAr, unity_SHAg, unity_SHAb, unity_SHBr, unity_SHBg, unity_SHBb, unity_SHC;
}

/// <summary>
/// The pipeline's globals as every Standard shader reads them: uniform slots 1 to 7 (slot 0 is the material's; slot 6
/// is the core's CascadedShadows), texture slots 8 and 9 (0 to 7 are the material's; 9 is the core's shadow atlas).
/// </summary>
public static class UniversalGlobals
{
    public const int MaxVisibleAdditionalLights = 256;

    [Uniform(1)] public static MainLightConstants mainLight;
    [Uniform(2)] public static AdditionalLightPositions lightPositions;
    [Uniform(3)] public static AdditionalLightColors lightColors;
    [Uniform(4)] public static AdditionalLightAttenuations lightAttenuations;
    [Uniform(5)] public static AdditionalLightSpotDirections lightSpotDirections;
    [Uniform(7)] public static AmbientProbeConstants ambient;

    /// <summary>The object's reflection probe cube (the scene's reflection without one).</summary>
    [Texture(8)] public static TextureCube<float4> unity_SpecCube0;
    [Sampler(8)] public static SamplerState samplerunity_SpecCube0;
}

/// <summary>What a surface shader gives the lighting (the reference's SurfaceData).</summary>
public struct SurfaceData
{
    public float3 albedo;
    public float3 specular;
    public float metallic;
    public float smoothness;
    public float3 normalTS;
    public float3 emission;
    public float occlusion;
    public float alpha;
}

/// <summary>Where and how a pixel is seen (the reference's InputData).</summary>
public struct InputData
{
    public float3 positionWS;
    public float3 normalWS;
    public float3 viewDirectionWS;
    public float3 bakedGI;
    public float2 normalizedScreenSpaceUV;
}

/// <summary>A light reaching a pixel.</summary>
public struct Light
{
    public float3 direction;
    public float3 color;
    public float distanceAttenuation;
    public float shadowAttenuation;
}

/// <summary>The microfacet values of a surface (the reference's BRDFData).</summary>
public struct BRDFData
{
    public float3 albedo;
    public float3 diffuse;
    public float3 specular;
    public float reflectivity;
    public float perceptualRoughness;
    public float roughness;
    public float roughness2;
    public float grazingTerm;
    public float normalizationTerm;
    public float roughness2MinusOne;
    /// <summary>The surface's alpha after the BRDF (premultiplied surfaces keep the specular's).</summary>
    public float alpha;
}
