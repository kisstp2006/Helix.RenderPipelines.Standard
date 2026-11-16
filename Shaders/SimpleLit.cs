using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>
/// Universal Render Pipeline/Simple Lit: Lambert's diffuse and, with a specular color (_SPECULAR_COLOR) or map
/// (_SPECGLOSSMAP), Blinn-Phong's highlights; its smoothness from the specular map's alpha or the base map's
/// (_GLOSSINESS_FROM_BASE_ALPHA); normal and emission maps, alpha clipping, the surface types and blend modes, render face,
/// receive shadows. Passes: ForwardLit, ShadowCaster, DepthOnly.
/// </summary>
[Shader("Universal Render Pipeline/Simple Lit", Fallback = "Hidden/Universal Render Pipeline/FallbackError")]
[Tag("RenderType", "Opaque")]
[Tag("RenderPipeline", "UniversalPipeline")]
public sealed class SimpleLit : ShaderDefinition
{
    [Property("Base Map"), MainTexture, DefaultTexture("white")] public Texture2D<float4> _BaseMap;
    [Property("Base Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 _BaseColor;
    [Property("Alpha Cutoff"), Range(0.0f, 1.0f), Default(0.5f)] public float _Cutoff;
    [Property("Smoothness"), Range(0.0f, 1.0f), Default(0.5f)] public float _Smoothness;
    [Property("Specular Color"), Default(0.5f, 0.5f, 0.5f, 0.5f)] public float4 _SpecColor;
    [Property("Specular Map"), DefaultTexture("white")] public Texture2D<float4> _SpecGlossMap;
    [Property("Smoothness Source"), Default(0.0f)] public float _SmoothnessSource;
    [Property("Specular Highlights"), Default(0.0f)] public float _SpecularHighlights;
    [Property("Normal Map"), DefaultTexture("bump")] public Texture2D<float4> _BumpMap;
    [Property("Emission Color"), HDR, Default(0.0f, 0.0f, 0.0f, 1.0f)] public float4 _EmissionColor;
    [Property("Emission Map"), DefaultTexture("white")] public Texture2D<float4> _EmissionMap;
    [Property("Surface Type"), Default(0.0f)] public float _Surface;
    [Property("Blending Mode"), Default(0.0f)] public float _Blend;
    [Property("Cull Mode"), Default(2.0f)] public float _Cull;
    [Property("Alpha Clipping"), Default(0.0f)] public float _AlphaClip;
    [Property("Src Blend"), Default(1.0f)] public float _SrcBlend;
    [Property("Dst Blend"), Default(0.0f)] public float _DstBlend;
    [Property("Src Blend Alpha"), Default(1.0f)] public float _SrcBlendAlpha;
    [Property("Dst Blend Alpha"), Default(0.0f)] public float _DstBlendAlpha;
    [Property("ZWrite"), Default(1.0f)] public float _ZWrite;
    [Property("Alpha To Mask"), Default(0.0f)] public float _AlphaToMask;
    [Property("Receive Shadows"), Default(1.0f)] public float _ReceiveShadows;
    [Property("Queue Offset"), Default(0.0f)] public float _QueueOffset;

    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _NORMALMAP;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHATEST_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SURFACE_TYPE_TRANSPARENT;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAPREMULTIPLY_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAMODULATE_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SPECGLOSSMAP;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SPECULAR_COLOR;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _GLOSSINESS_FROM_BASE_ALPHA;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _EMISSION;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _RECEIVE_SHADOWS_OFF;

    public struct MaterialData
    {
        public float4 _BaseMap_ST;
        public float4 _BaseColor;
        public float4 _SpecColor;
        public float4 _EmissionColor;
        public float _Cutoff;
        public float _Smoothness;
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

    [Pass("ForwardLit", LightMode = "UniversalForward", CullProperty = "_Cull", ZWriteProperty = "_ZWrite", AlphaToMaskProperty = "_AlphaToMask")]
    [Blend(SourceProperty = "_SrcBlend", DestinationProperty = "_DstBlend", SourceAlphaProperty = "_SrcBlendAlpha", DestinationAlphaProperty = "_DstBlendAlpha")]
    public static class ForwardLit
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;
        [Texture(1)] public static Texture2D<float4> _SpecGlossMap;
        [Sampler(1)] public static SamplerState sampler_SpecGlossMap;
        [Texture(2)] public static Texture2D<float4> _BumpMap;
        [Sampler(2)] public static SamplerState sampler_BumpMap;
        [Texture(3)] public static Texture2D<float4> _EmissionMap;
        [Sampler(3)] public static SamplerState sampler_EmissionMap;

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
            float4 albedoAlpha = _BaseMap.Sample(sampler_BaseMap, uv);
            SurfaceData s;
            s.alpha = UniversalLighting.Alpha(albedoAlpha.w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
            s.albedo = albedoAlpha.xyz * material._BaseColor.xyz;
            s.metallic = 0.0f;
            s.smoothness = material._Smoothness;
            s.specular = new float3(0.0f);
            s.occlusion = 1.0f;
            s.emission = _EMISSION ? _EmissionMap.Sample(sampler_EmissionMap, uv).xyz * material._EmissionColor.xyz : new float3(0.0f);
            s.normalTS = _NORMALMAP ? UniversalLighting.UnpackNormalScale(_BumpMap.Sample(sampler_BumpMap, uv), 1.0f) : new float3(0.0f, 0.0f, 1.0f);
            // The specular color and its smoothness: the map's, else the color's; the smoothness from the base map's alpha
            // when asked.
            float4 specularGloss = _SPECGLOSSMAP ? _SpecGlossMap.Sample(sampler_SpecGlossMap, uv) * material._SpecColor : material._SpecColor;
            float smoothness = (_GLOSSINESS_FROM_BASE_ALPHA ? albedoAlpha.w : specularGloss.w) * material._Smoothness;
            if (_ALPHAPREMULTIPLY_ON)
                s.albedo *= s.alpha;
            InputData d;
            d.positionWS = input.positionWS;
            d.normalWS = _NORMALMAP ? UniversalLighting.TangentToWorld(s.normalTS, input.normalWS, input.tangentWS) : Normalize(input.normalWS);
            d.viewDirectionWS = UniversalLighting.ViewDirection(input.positionWS);
            d.bakedGI = UniversalLighting.SampleSH(d.normalWS);
            d.normalizedScreenSpaceUV = new float2(0.0f);
            float4 color = UniversalLighting.UniversalFragmentBlinnPhong(d, s, specularGloss, smoothness, _SPECULAR_COLOR || _SPECGLOSSMAP, _RECEIVE_SHADOWS_OFF,
                                                                         _SURFACE_TYPE_TRANSPARENT);
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
