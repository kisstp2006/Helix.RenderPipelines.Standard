using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>
/// Universal Render Pipeline/Lit: physically based shading in the metallic or the specular workflow (_SPECULAR_SETUP),
/// with base, metallic or specular gloss, normal, occlusion and emission maps, the smoothness from the gloss map's or the
/// base map's alpha, alpha clipping, opaque or transparent surfaces blended as the material's properties say (alpha,
/// premultiply, additive, multiply), its render face, receive shadows, specular highlights and environment reflections
/// switches. Passes: ForwardLit, ShadowCaster, DepthOnly.
/// </summary>
[Shader("Universal Render Pipeline/Lit", Fallback = "Hidden/Universal Render Pipeline/FallbackError")]
[Tag("RenderType", "Opaque")]
[Tag("RenderPipeline", "UniversalPipeline")]
public sealed class Lit : ShaderDefinition
{
    [Property("Workflow Mode"), Default(1.0f)] public float _WorkflowMode;
    [Property("Base Map"), MainTexture, DefaultTexture("white")] public Texture2D<float4> _BaseMap;
    [Property("Base Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 _BaseColor;
    [Property("Alpha Cutoff"), Range(0.0f, 1.0f), Default(0.5f)] public float _Cutoff;
    [Property("Smoothness"), Range(0.0f, 1.0f), Default(0.5f)] public float _Smoothness;
    [Property("Smoothness Scale"), Range(0.0f, 1.0f), Default(1.0f)] public float _GlossMapScale;
    [Property("Smoothness texture channel"), Default(0.0f)] public float _SmoothnessTextureChannel;
    [Property("Metallic"), Range(0.0f, 1.0f), Default(0.0f)] public float _Metallic;
    [Property("Metallic"), DefaultTexture("white")] public Texture2D<float4> _MetallicGlossMap;
    [Property("Specular"), Default(0.2f, 0.2f, 0.2f, 1.0f)] public float4 _SpecColor;
    [Property("Specular"), DefaultTexture("white")] public Texture2D<float4> _SpecGlossMap;
    [Property("Specular Highlights"), Default(1.0f)] public float _SpecularHighlights;
    [Property("Environment Reflections"), Default(1.0f)] public float _EnvironmentReflections;
    [Property("Scale"), Default(1.0f)] public float _BumpScale;
    [Property("Normal Map"), DefaultTexture("bump")] public Texture2D<float4> _BumpMap;
    [Property("Strength"), Range(0.0f, 1.0f), Default(1.0f)] public float _OcclusionStrength;
    [Property("Occlusion"), DefaultTexture("white")] public Texture2D<float4> _OcclusionMap;
    [Property("Color"), HDR, Default(0.0f, 0.0f, 0.0f, 1.0f)] public float4 _EmissionColor;
    [Property("Emission"), DefaultTexture("white")] public Texture2D<float4> _EmissionMap;
    // Surface options (the states of the passes come from them).
    [Property("Surface Type"), Default(0.0f)] public float _Surface;
    [Property("Blending Mode"), Default(0.0f)] public float _Blend;
    [Property("Cull Mode"), Default(2.0f)] public float _Cull;
    [Property("Alpha Clipping"), Default(0.0f)] public float _AlphaClip;
    [Property("Src Blend"), Default(1.0f)] public float _SrcBlend;
    [Property("Dst Blend"), Default(0.0f)] public float _DstBlend;
    [Property("Src Blend Alpha"), Default(1.0f)] public float _SrcBlendAlpha;
    [Property("Dst Blend Alpha"), Default(0.0f)] public float _DstBlendAlpha;
    [Property("ZWrite"), Default(1.0f)] public float _ZWrite;
    [Property("Preserve Specular"), Default(1.0f)] public float _BlendModePreserveSpecular;
    [Property("Alpha To Mask"), Default(0.0f)] public float _AlphaToMask;
    [Property("Receive Shadows"), Default(1.0f)] public float _ReceiveShadows;
    [Property("Queue Offset"), Default(0.0f)] public float _QueueOffset;

    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _NORMALMAP;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHATEST_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SURFACE_TYPE_TRANSPARENT;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAPREMULTIPLY_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAMODULATE_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SPECULAR_SETUP;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _METALLICSPECGLOSSMAP;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _OCCLUSIONMAP;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _EMISSION;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _RECEIVE_SHADOWS_OFF;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SPECULARHIGHLIGHTS_OFF;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ENVIRONMENTREFLECTIONS_OFF;

    public struct MaterialData
    {
        public float4 _BaseMap_ST;
        public float4 _BaseColor;
        public float4 _SpecColor;
        public float4 _EmissionColor;
        public float _Cutoff;
        public float _Smoothness;
        public float _Metallic;
        public float _BumpScale;
        public float _OcclusionStrength;
        public float _Surface;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float2 uv;
        [Location(1)] public float3 positionWS;
        [Location(2)] public float3 normalWS;
        [Location(3)] public float4 tangentWS;
    }

    public struct DepthVaryings
    {
        [Position] public float4 position;
        [Location(0)] public float2 uv;
    }

    [Uniform(0)] public static MaterialData material;

    static float2 BaseUV(float2 uv) => uv * material._BaseMap_ST.xy + material._BaseMap_ST.zw;

    /// <summary>The surface of a pixel: the base color and alpha (clipped), metallic or specular, smoothness, the normal
    /// from the normal map, occlusion, emission.</summary>
    static SurfaceData Surface(float2 uv, float4 albedoAlpha, float4 metallicGloss, float4 specGloss, float4 bump, float4 occlusionMap, float4 emissionMap)
    {
        SurfaceData s;
        s.alpha = UniversalLighting.Alpha(albedoAlpha.w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
        s.albedo = albedoAlpha.xyz * material._BaseColor.xyz;
        float4 gloss;
        if (_SPECULAR_SETUP)
            gloss = _METALLICSPECGLOSSMAP ? specGloss : new float4(material._SpecColor.xyz, 1.0f);
        else
            gloss = _METALLICSPECGLOSSMAP ? metallicGloss : new float4(material._Metallic, 0.0f, 0.0f, 1.0f);
        float smoothnessSource = _SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A ? albedoAlpha.w : (_METALLICSPECGLOSSMAP ? gloss.w : 1.0f);
        s.smoothness = smoothnessSource * material._Smoothness;
        s.metallic = _SPECULAR_SETUP ? 1.0f : gloss.x;
        s.specular = _SPECULAR_SETUP ? gloss.xyz : new float3(0.0f);
        s.normalTS = _NORMALMAP ? UniversalLighting.UnpackNormalScale(bump, material._BumpScale) : new float3(0.0f, 0.0f, 1.0f);
        s.occlusion = _OCCLUSIONMAP ? Lerp(1.0f, occlusionMap.y, material._OcclusionStrength) : 1.0f;
        s.emission = _EMISSION ? emissionMap.xyz * material._EmissionColor.xyz : new float3(0.0f);
        return s;
    }

    [Pass("ForwardLit", LightMode = "UniversalForward", CullProperty = "_Cull", ZWriteProperty = "_ZWrite", AlphaToMaskProperty = "_AlphaToMask")]
    [Blend(SourceProperty = "_SrcBlend", DestinationProperty = "_DstBlend", SourceAlphaProperty = "_SrcBlendAlpha", DestinationAlphaProperty = "_DstBlendAlpha")]
    public static class ForwardLit
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;
        [Texture(1)] public static Texture2D<float4> _MetallicGlossMap;
        [Sampler(1)] public static SamplerState sampler_MetallicGlossMap;
        [Texture(2)] public static Texture2D<float4> _SpecGlossMap;
        [Sampler(2)] public static SamplerState sampler_SpecGlossMap;
        [Texture(3)] public static Texture2D<float4> _BumpMap;
        [Sampler(3)] public static SamplerState sampler_BumpMap;
        [Texture(4)] public static Texture2D<float4> _OcclusionMap;
        [Sampler(4)] public static SamplerState sampler_OcclusionMap;
        [Texture(5)] public static Texture2D<float4> _EmissionMap;
        [Sampler(5)] public static SamplerState sampler_EmissionMap;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Normal)] float3 normal,
                                      [Location(VertexInputs.Tangent)] float4 tangent, [Location(VertexInputs.TexCoord0)] float2 uv)
        {
            Varyings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.uv = BaseUV(uv);
            o.positionWS = ShaderVariables.ObjectToWorld(position);
            o.normalWS = ShaderVariables.ObjectToWorldNormal(normal);
            o.tangentWS = UniversalLighting.ObjectToWorldTangent(tangent);
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float2 uv = input.uv;
            SurfaceData s = Surface(uv, _BaseMap.Sample(sampler_BaseMap, uv), _MetallicGlossMap.Sample(sampler_MetallicGlossMap, uv),
                                    _SpecGlossMap.Sample(sampler_SpecGlossMap, uv), _BumpMap.Sample(sampler_BumpMap, uv),
                                    _OcclusionMap.Sample(sampler_OcclusionMap, uv), _EmissionMap.Sample(sampler_EmissionMap, uv));
            InputData d;
            d.positionWS = input.positionWS;
            d.normalWS = _NORMALMAP ? UniversalLighting.TangentToWorld(s.normalTS, input.normalWS, input.tangentWS) : Normalize(input.normalWS);
            d.viewDirectionWS = UniversalLighting.ViewDirection(input.positionWS);
            d.bakedGI = UniversalLighting.SampleSH(d.normalWS);
            d.normalizedScreenSpaceUV = new float2(0.0f);
            float4 color = UniversalLighting.UniversalFragmentPBR(d, s, _SPECULAR_SETUP, _ALPHAPREMULTIPLY_ON, _SPECULARHIGHLIGHTS_OFF, _ENVIRONMENTREFLECTIONS_OFF,
                                                                  _RECEIVE_SHADOWS_OFF, _SURFACE_TYPE_TRANSPARENT);
            return new float4(UniversalLighting.AlphaModulate(color.xyz, color.w, _ALPHAMODULATE_ON), color.w);
        }
    }

    [Pass("ShadowCaster", LightMode = "ShadowCaster", ZWrite = true, ZTest = CompareFunction.LessEqual, ColorMask = ColorWriteMask.None, CullProperty = "_Cull")]
    public static class ShadowCaster
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;

        [VertexShader]
        public static DepthVaryings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Normal)] float3 normal,
                                           [Location(VertexInputs.TexCoord0)] float2 uv)
        {
            DepthVaryings o;
            o.position = UniversalShadowCaster.ShadowPositionCS(position, normal);
            o.uv = BaseUV(uv);
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(DepthVaryings input)
        {
            UniversalLighting.Alpha(_BaseMap.Sample(sampler_BaseMap, input.uv).w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
            return new float4(0.0f);
        }
    }

    [Pass("DepthOnly", LightMode = "DepthOnly", ZWrite = true, ColorMask = ColorWriteMask.Red, CullProperty = "_Cull")]
    public static class DepthOnly
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;

        [VertexShader]
        public static DepthVaryings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.TexCoord0)] float2 uv)
        {
            DepthVaryings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.uv = BaseUV(uv);
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(DepthVaryings input)
        {
            UniversalLighting.Alpha(_BaseMap.Sample(sampler_BaseMap, input.uv).w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
            return new float4(input.position.z, 0.0f, 0.0f, 0.0f);
        }
    }
}
