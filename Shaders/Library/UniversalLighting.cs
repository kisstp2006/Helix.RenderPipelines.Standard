using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>
/// The pipeline's lighting (the reference's Lighting, BRDF, GlobalIllumination and RealtimeLights): the BRDF of the
/// metallic and specular workflows with GGX and the cheap visibility and Fresnel term, Lambert and Blinn-Phong, the
/// environment BRDF with the reflection probe, the ambient probe, the main light and the additional lights of an
/// object. Keyword states come in as arguments (the shaders' keywords are constants of their variants).
/// </summary>
public static class UniversalLighting
{
    const float kDielectricSpecular = 0.04f;
    const float HalfMin = 6.103515625e-5f;
    const float HalfMinSqrt = 0.0078125f;
    // The mip levels a probe's roughness spans.
    const float SpecCubeLodSteps = 6.0f;

    // ------------------------------------------------------------------------------------------------ BRDF

    public static float OneMinusReflectivityMetallic(float metallic) => (1.0f - kDielectricSpecular) - metallic * (1.0f - kDielectricSpecular);

    /// <summary>The surface's BRDF values: specular from _SpecColor or a map in the specular workflow, from the metallic
    /// value in the metallic one; the diffuse premultiplied by alpha when the surface is.</summary>
    public static BRDFData InitializeBRDFData(SurfaceData s, bool specularSetup, bool alphaPremultiply)
    {
        BRDFData b;
        float oneMinusReflectivity;
        if (specularSetup)
        {
            b.reflectivity = Max(Max(s.specular.x, s.specular.y), s.specular.z);
            oneMinusReflectivity = 1.0f - b.reflectivity;
            b.specular = s.specular;
        }
        else
        {
            oneMinusReflectivity = OneMinusReflectivityMetallic(s.metallic);
            b.reflectivity = 1.0f - oneMinusReflectivity;
            b.specular = Lerp(new float3(kDielectricSpecular), s.albedo, s.metallic);
        }
        b.albedo = s.albedo;
        b.diffuse = s.albedo * oneMinusReflectivity;
        b.perceptualRoughness = 1.0f - s.smoothness;
        b.roughness = Max(b.perceptualRoughness * b.perceptualRoughness, HalfMinSqrt);
        b.roughness2 = Max(b.roughness * b.roughness, HalfMin);
        b.grazingTerm = Saturate(s.smoothness + b.reflectivity);
        b.normalizationTerm = b.roughness * 4.0f + 2.0f;
        b.roughness2MinusOne = b.roughness2 - 1.0f;
        b.alpha = s.alpha;
        if (alphaPremultiply)
        {
            b.diffuse *= s.alpha;
            b.alpha = s.alpha * oneMinusReflectivity + b.reflectivity;
        }
        return b;
    }

    /// <summary>The specular of a light: GGX's distribution with the cheap visibility and Fresnel term.</summary>
    public static float DirectBRDFSpecular(BRDFData b, float3 normalWS, float3 lightDirectionWS, float3 viewDirectionWS)
    {
        float3 h = SafeNormalize(lightDirectionWS + viewDirectionWS);
        float NoH = Saturate(Dot(normalWS, h));
        float LoH = Saturate(Dot(lightDirectionWS, h));
        float d = NoH * NoH * b.roughness2MinusOne + 1.00001f;
        float LoH2 = LoH * LoH;
        return b.roughness2 / ((d * d) * Max(0.1f, LoH2) * b.normalizationTerm);
    }

    public static float3 SafeNormalize(float3 v) => v * Rsqrt(Max(Dot(v, v), 1.0e-12f));

    public static float3 LightingPhysicallyBased(BRDFData b, Light light, float3 normalWS, float3 viewDirectionWS, bool specularHighlightsOff)
    {
        float NdotL = Saturate(Dot(normalWS, light.direction));
        float3 radiance = light.color * (light.distanceAttenuation * light.shadowAttenuation * NdotL);
        float3 brdf = b.diffuse;
        if (!specularHighlightsOff)
            brdf += b.specular * DirectBRDFSpecular(b, normalWS, light.direction, viewDirectionWS);
        return brdf * radiance;
    }

    // ------------------------------------------------------------------------------------------------ simple lighting

    public static float3 LightingLambert(float3 lightColor, float3 lightDirection, float3 normal) => lightColor * Saturate(Dot(normal, lightDirection));

    /// <summary>Blinn-Phong's highlight (smoothness: the exponent).</summary>
    public static float3 LightingSpecular(float3 lightColor, float3 lightDirection, float3 normal, float3 viewDirection, float4 specular, float smoothness)
    {
        float3 h = SafeNormalize(lightDirection + viewDirection);
        float NdotH = Saturate(Dot(normal, h));
        return lightColor * (specular.xyz * Pow(NdotH, smoothness));
    }

    // ------------------------------------------------------------------------------------------------ environment

    /// <summary>The ambient probe's light of a direction (the reference's SampleSH).</summary>
    public static float3 SampleSH(float3 n)
    {
        AmbientProbeConstants a = UniversalGlobals.ambient;
        float4 n1 = new float4(n, 1.0f);
        float3 l1 = new float3(Dot(a.unity_SHAr, n1), Dot(a.unity_SHAg, n1), Dot(a.unity_SHAb, n1));
        float4 vB = new float4(n.x * n.y, n.y * n.z, n.z * n.z, n.z * n.x);
        float3 l2 = new float3(Dot(a.unity_SHBr, vB), Dot(a.unity_SHBg, vB), Dot(a.unity_SHBb, vB));
        float vC = n.x * n.x - n.y * n.y;
        return Max(new float3(0.0f), l1 + l2 + a.unity_SHC.xyz * vC);
    }

    public static float3 DecodeHDREnvironment(float4 encoded, float4 decodeInstructions)
    {
        float alpha = decodeInstructions.w * (encoded.w - 1.0f) + 1.0f;
        return decodeInstructions.x * Pow(Abs(alpha), decodeInstructions.y) * encoded.xyz;
    }

    /// <summary>A reflection direction into a box projected probe: where it leaves the probe's box, seen from the probe.</summary>
    public static float3 BoxProjectedCubemapDirection(float3 reflectVector, float3 positionWS, float4 probePosition, float4 boxMin, float4 boxMax)
    {
        float3 lo = (boxMin.xyz - positionWS) / reflectVector;
        float3 hi = (boxMax.xyz - positionWS) / reflectVector;
        float3 factors = new float3(reflectVector.x > 0.0f ? hi.x : lo.x, reflectVector.y > 0.0f ? hi.y : lo.y, reflectVector.z > 0.0f ? hi.z : lo.z);
        float scalar = Min(Min(factors.x, factors.y), factors.z);
        return reflectVector * scalar + (positionWS - probePosition.xyz);
    }

    public static float PerceptualRoughnessToMipmapLevel(float perceptualRoughness) => perceptualRoughness * (1.7f - 0.7f * perceptualRoughness) * SpecCubeLodSteps;

    /// <summary>The reflection probe's light along the reflection (the probe's box projection, its roughness's mip).</summary>
    public static float3 GlossyEnvironmentReflection(float3 reflectVector, float3 positionWS, float perceptualRoughness, float occlusion, bool reflectionsOff)
    {
        if (reflectionsOff)
            return UniversalGlobals.mainLight._GlossyEnvironmentColor.xyz * occlusion;
        float4 probePosition = ShaderVariables.SpecCube0ProbePosition();
        float3 direction = reflectVector;
        if (probePosition.w > 0.0f)
            direction = BoxProjectedCubemapDirection(reflectVector, positionWS, probePosition, ShaderVariables.SpecCube0BoxMin(), ShaderVariables.SpecCube0BoxMax());
        float mip = PerceptualRoughnessToMipmapLevel(perceptualRoughness);
        float4 encoded = UniversalGlobals.unity_SpecCube0.SampleLevel(UniversalGlobals.samplerunity_SpecCube0, direction, mip);
        return DecodeHDREnvironment(encoded, ShaderVariables.SpecCube0HDR()) * occlusion;
    }

    public static float3 EnvironmentBRDF(BRDFData b, float3 indirectDiffuse, float3 indirectSpecular, float fresnelTerm)
    {
        float3 c = indirectDiffuse * b.diffuse;
        float surfaceReduction = 1.0f / (b.roughness2 + 1.0f);
        return c + indirectSpecular * (surfaceReduction * Lerp(b.specular, new float3(b.grazingTerm), fresnelTerm));
    }

    public static float3 GlobalIllumination(BRDFData b, float3 bakedGI, float occlusion, float3 positionWS, float3 normalWS, float3 viewDirectionWS, bool reflectionsOff)
    {
        float3 reflectVector = Reflect(-viewDirectionWS, normalWS);
        float NoV = Saturate(Dot(normalWS, viewDirectionWS));
        float f = 1.0f - NoV;
        float fresnelTerm = f * f * f * f;
        float3 indirectDiffuse = bakedGI;
        float3 indirectSpecular = GlossyEnvironmentReflection(reflectVector, positionWS, b.perceptualRoughness, 1.0f, reflectionsOff);
        return EnvironmentBRDF(b, indirectDiffuse, indirectSpecular, fresnelTerm) * occlusion;
    }

    // ------------------------------------------------------------------------------------------------ lights

    /// <summary>The main light (the brightest directional one) with its shadow at the position.</summary>
    public static Light GetMainLight(float3 positionWS, bool receiveShadowsOff)
    {
        Light l;
        l.direction = UniversalGlobals.mainLight._MainLightPosition.xyz;
        l.color = UniversalGlobals.mainLight._MainLightColor.xyz;
        l.distanceAttenuation = 1.0f;
        l.shadowAttenuation = receiveShadowsOff ? 1.0f : UniversalShadows.MainLightRealtimeShadow(positionWS);
        return l;
    }

    /// <summary>The fade of a point or spot light by distance: 1 / d^2, smoothly to zero at its range.</summary>
    public static float DistanceAttenuation(float distanceSqr, float2 distanceAttenuation)
    {
        float lightAtten = 1.0f / distanceSqr;
        float factor = distanceSqr * distanceAttenuation.x;
        float smoothFactor = Saturate(1.0f - factor * factor);
        return lightAtten * smoothFactor * smoothFactor;
    }

    /// <summary>A spot light's falloff between its inner and outer angles.</summary>
    public static float AngleAttenuation(float3 spotDirection, float3 lightDirection, float2 spotAttenuation)
    {
        float SdotL = Dot(spotDirection, lightDirection);
        float atten = Saturate(SdotL * spotAttenuation.x + spotAttenuation.y);
        return atten * atten;
    }

    /// <summary>The object's additional lights (by its light indices, ShaderVariables.ObjectLightCount of them).</summary>
    public static int GetAdditionalLightsCount() => Min(ShaderVariables.ObjectLightCount(), (int)UniversalGlobals.mainLight._AdditionalLightsCount.x);

    public static Light GetAdditionalLight(int i, float3 positionWS)
    {
        int index = ShaderVariables.ObjectLightIndex(i);
        float4 position = UniversalGlobals.lightPositions._AdditionalLightsPosition[index];
        float4 attenuation = UniversalGlobals.lightAttenuations._AdditionalLightsAttenuation[index];
        float4 spotDirection = UniversalGlobals.lightSpotDirections._AdditionalLightsSpotDir[index];
        float3 toLight = position.xyz - positionWS * position.w;
        float distanceSqr = Max(Dot(toLight, toLight), HalfMin);
        float3 direction = toLight * Rsqrt(distanceSqr);
        Light l;
        l.direction = direction;
        l.color = UniversalGlobals.lightColors._AdditionalLightsColor[index].xyz;
        l.distanceAttenuation = DistanceAttenuation(distanceSqr, attenuation.xy) * AngleAttenuation(spotDirection.xyz, direction, attenuation.zw);
        l.shadowAttenuation = 1.0f;
        return l;
    }

    // ------------------------------------------------------------------------------------------------ the fragment

    /// <summary>The physically based color of a surface: its environment, the main light, its additional lights and its
    /// emission; the alpha of the surface (1 for opaque ones).</summary>
    public static float4 UniversalFragmentPBR(InputData input, SurfaceData s, bool specularSetup, bool alphaPremultiply, bool specularHighlightsOff,
                                              bool reflectionsOff, bool receiveShadowsOff, bool transparent)
    {
        BRDFData b = InitializeBRDFData(s, specularSetup, alphaPremultiply);
        Light main = GetMainLight(input.positionWS, receiveShadowsOff);
        float3 color = GlobalIllumination(b, input.bakedGI, s.occlusion, input.positionWS, input.normalWS, input.viewDirectionWS, reflectionsOff);
        color += LightingPhysicallyBased(b, main, input.normalWS, input.viewDirectionWS, specularHighlightsOff);
        int count = GetAdditionalLightsCount();
        for (int i = 0; i < 8; i++)
        {
            if (i >= count)
                break;
            Light l = GetAdditionalLight(i, input.positionWS);
            color += LightingPhysicallyBased(b, l, input.normalWS, input.viewDirectionWS, specularHighlightsOff);
        }
        color += s.emission;
        return new float4(color, transparent ? b.alpha : 1.0f);
    }

    /// <summary>Lambert's diffuse and, with a specular color, Blinn-Phong's highlights (smoothness 0 to 1 as an exponent
    /// of 2 to 2048); the ambient probe; the emission.</summary>
    public static float4 UniversalFragmentBlinnPhong(InputData input, SurfaceData s, float4 specularGloss, float smoothness, bool specularOn, bool receiveShadowsOff,
                                                     bool transparent)
    {
        float exponent = Exp2(10.0f * smoothness + 1.0f);
        Light main = GetMainLight(input.positionWS, receiveShadowsOff);
        float3 attenuated = main.color * (main.distanceAttenuation * main.shadowAttenuation);
        float3 diffuse = input.bakedGI * s.occlusion + LightingLambert(attenuated, main.direction, input.normalWS);
        float3 specular = new float3(0.0f);
        if (specularOn)
            specular += LightingSpecular(attenuated, main.direction, input.normalWS, input.viewDirectionWS, specularGloss, exponent);
        int count = GetAdditionalLightsCount();
        for (int i = 0; i < 8; i++)
        {
            if (i >= count)
                break;
            Light l = GetAdditionalLight(i, input.positionWS);
            float3 a = l.color * (l.distanceAttenuation * l.shadowAttenuation);
            diffuse += LightingLambert(a, l.direction, input.normalWS);
            if (specularOn)
                specular += LightingSpecular(a, l.direction, input.normalWS, input.viewDirectionWS, specularGloss, exponent);
        }
        float3 color = diffuse * s.albedo + specular + s.emission;
        return new float4(color, transparent ? s.alpha : 1.0f);
    }

    // ------------------------------------------------------------------------------------------------ surfaces

    /// <summary>A tangent space normal from a normal map's texel (RGB), its xy scaled.</summary>
    public static float3 UnpackNormalScale(float4 packedNormal, float scale)
    {
        float3 n = packedNormal.xyz * 2.0f - new float3(1.0f);
        return new float3(n.xy * scale, n.z);
    }

    /// <summary>A tangent space normal in world space (the vertex's tangent, its sign in w, and normal).</summary>
    public static float3 TangentToWorld(float3 normalTS, float3 normalWS, float4 tangentWS)
    {
        float3 n = Normalize(normalWS);
        float3 t = Normalize(tangentWS.xyz);
        float3 b = Cross(n, t) * (tangentWS.w > 0.0f ? 1.0f : -1.0f);
        return Normalize(t * normalTS.x + b * normalTS.y + n * normalTS.z);
    }

    /// <summary>The surface's alpha, clipped at the cutoff with alpha clipping on.</summary>
    public static float Alpha(float albedoAlpha, float4 color, float cutoff, bool alphaTest)
    {
        float alpha = albedoAlpha * color.w;
        if (alphaTest && alpha - cutoff < 0.0f)
            Discard();
        return alpha;
    }

    /// <summary>The multiply blend mode's color: white where the surface is transparent.</summary>
    public static float3 AlphaModulate(float3 color, float alpha, bool alphaModulate) => alphaModulate ? Lerp(new float3(1.0f), color, alpha) : color;

    /// <summary>The view direction from a world position (toward the camera).</summary>
    public static float3 ViewDirection(float3 positionWS) => SafeNormalize(ShaderVariables.camera.helix_WorldSpaceCameraPos.xyz - positionWS);

    /// <summary>The object's tangent (w: the bitangent's sign) in world space.</summary>
    public static float4 ObjectToWorldTangent(float4 tangent) =>
        new float4(Normalize(Mul(ShaderVariables.ObjectToWorldMatrix(), new float4(tangent.xyz, 0.0f)).xyz), tangent.w);
}
