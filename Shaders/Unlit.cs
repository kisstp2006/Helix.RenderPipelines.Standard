using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>
/// Universal Render Pipeline/Unlit: the base map times the base color, unlit; alpha clipping, the surface types and
/// blend modes, render face. Passes: Unlit (the SRPDefaultUnlit light mode), ShadowCaster, DepthOnly.
/// </summary>
[Shader("Universal Render Pipeline/Unlit", Fallback = "Hidden/Universal Render Pipeline/FallbackError")]
[Tag("RenderType", "Opaque")]
[Tag("RenderPipeline", "UniversalPipeline")]
public sealed class Unlit : ShaderDefinition
{
    [Property("Texture"), MainTexture, DefaultTexture("white")] public Texture2D<float4> _BaseMap;
    [Property("Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 _BaseColor;
    [Property("AlphaClip"), Range(0.0f, 1.0f), Default(0.5f)] public float _Cutoff;
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
    [Property("Queue Offset"), Default(0.0f)] public float _QueueOffset;

    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHATEST_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SURFACE_TYPE_TRANSPARENT;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAPREMULTIPLY_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAMODULATE_ON;

    public struct MaterialData
    {
        public float4 _BaseMap_ST;
        public float4 _BaseColor;
        public float _Cutoff;
        public float _Surface;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float2 uv;
    }

    [Uniform(0)] public static MaterialData material;

    static float2 BaseUV(float2 uv) => uv * material._BaseMap_ST.xy + material._BaseMap_ST.zw;

    static Varyings Transform(float3 position, float2 uv)
    {
        Varyings o;
        o.position = ShaderVariables.ObjectToClip(position);
        o.uv = BaseUV(uv);
        return o;
    }

    [Pass("Unlit", CullProperty = "_Cull", ZWriteProperty = "_ZWrite", AlphaToMaskProperty = "_AlphaToMask")]
    [Blend(SourceProperty = "_SrcBlend", DestinationProperty = "_DstBlend", SourceAlphaProperty = "_SrcBlendAlpha", DestinationAlphaProperty = "_DstBlendAlpha")]
    public static class Forward
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.TexCoord0)] float2 uv) => Transform(position, uv);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float4 texel = _BaseMap.Sample(sampler_BaseMap, input.uv);
            float alpha = UniversalLighting.Alpha(texel.w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
            float3 color = texel.xyz * material._BaseColor.xyz;
            if (_ALPHAPREMULTIPLY_ON)
                color *= alpha;
            color = UniversalLighting.AlphaModulate(color, alpha, _ALPHAMODULATE_ON);
            return new float4(color, _SURFACE_TYPE_TRANSPARENT ? alpha : 1.0f);
        }
    }

    [Pass("ShadowCaster", LightMode = "ShadowCaster", ZWrite = true, ZTest = CompareFunction.LessEqual, ColorMask = ColorWriteMask.None, CullProperty = "_Cull")]
    public static class ShadowCaster
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Normal)] float3 normal,
                                      [Location(VertexInputs.TexCoord0)] float2 uv)
        {
            Varyings o;
            o.position = UniversalShadowCaster.ShadowPositionCS(position, normal);
            o.uv = BaseUV(uv);
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
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
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.TexCoord0)] float2 uv) => Transform(position, uv);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            UniversalLighting.Alpha(_BaseMap.Sample(sampler_BaseMap, input.uv).w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
            return new float4(input.position.z, 0.0f, 0.0f, 0.0f);
        }
    }
}

/// <summary>
/// Universal Render Pipeline/Particles/Unlit: the base map times the base color and the vertex color, unlit, in the
/// surface types and blend modes (what the pipeline's default particle and line materials draw with). Passes: Unlit,
/// DepthOnly.
/// </summary>
[Shader("Universal Render Pipeline/Particles/Unlit", Fallback = "Hidden/Universal Render Pipeline/FallbackError")]
[Tag("RenderType", "Opaque")]
[Tag("RenderPipeline", "UniversalPipeline")]
public sealed class ParticlesUnlit : ShaderDefinition
{
    [Property("Base Map"), MainTexture, DefaultTexture("white")] public Texture2D<float4> _BaseMap;
    [Property("Base Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 _BaseColor;
    [Property("Alpha Cutoff"), Range(0.0f, 1.0f), Default(0.5f)] public float _Cutoff;
    [Property("Surface Type"), Default(0.0f)] public float _Surface;
    [Property("Blending Mode"), Default(0.0f)] public float _Blend;
    [Property("Cull Mode"), Default(2.0f)] public float _Cull;
    [Property("Src Blend"), Default(1.0f)] public float _SrcBlend;
    [Property("Dst Blend"), Default(0.0f)] public float _DstBlend;
    [Property("ZWrite"), Default(1.0f)] public float _ZWrite;

    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHATEST_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _SURFACE_TYPE_TRANSPARENT;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAPREMULTIPLY_ON;
    [Keyword(KeywordScope.Local, ShaderFeature = true)] public static readonly bool _ALPHAMODULATE_ON;

    public struct MaterialData
    {
        public float4 _BaseMap_ST;
        public float4 _BaseColor;
        public float _Cutoff;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float2 uv;
        [Location(1)] public float4 color;
    }

    [Uniform(0)] public static MaterialData material;

    static Varyings Transform(float3 position, float4 color, float2 uv)
    {
        Varyings o;
        o.position = ShaderVariables.ObjectToClip(position);
        o.uv = uv * material._BaseMap_ST.xy + material._BaseMap_ST.zw;
        o.color = color;
        return o;
    }

    [Pass("Unlit", CullProperty = "_Cull", ZWriteProperty = "_ZWrite")]
    [Blend(SourceProperty = "_SrcBlend", DestinationProperty = "_DstBlend")]
    public static class Forward
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Color)] float4 color,
                                      [Location(VertexInputs.TexCoord0)] float2 uv) => Transform(position, color, uv);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float4 texel = _BaseMap.Sample(sampler_BaseMap, input.uv) * input.color;
            float alpha = UniversalLighting.Alpha(texel.w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
            float3 color = texel.xyz * material._BaseColor.xyz;
            if (_ALPHAPREMULTIPLY_ON)
                color *= alpha;
            color = UniversalLighting.AlphaModulate(color, alpha, _ALPHAMODULATE_ON);
            return new float4(color, _SURFACE_TYPE_TRANSPARENT ? alpha : 1.0f);
        }
    }

    [Pass("DepthOnly", LightMode = "DepthOnly", ZWrite = true, ColorMask = ColorWriteMask.Red, CullProperty = "_Cull")]
    public static class DepthOnly
    {
        [Texture(0)] public static Texture2D<float4> _BaseMap;
        [Sampler(0)] public static SamplerState sampler_BaseMap;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Color)] float4 color,
                                      [Location(VertexInputs.TexCoord0)] float2 uv) => Transform(position, color, uv);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            UniversalLighting.Alpha((_BaseMap.Sample(sampler_BaseMap, input.uv) * input.color).w, material._BaseColor, material._Cutoff, _ALPHATEST_ON);
            return new float4(input.position.z, 0.0f, 0.0f, 0.0f);
        }
    }
}
