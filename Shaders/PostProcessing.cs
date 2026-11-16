using HelixEngine.Rendering.ShaderLibrary;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;
using LibColor = HelixEngine.Rendering.ShaderLibrary.Color;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>
/// What the post processing shaders share: the Blitter's full screen triangle, the conversions between the camera's
/// colors (the engine draws in gamma space) and linear ones, and the color grading of the look up table builders.
/// </summary>
public static class PostProcessingCommon
{
    public struct BlitData
    {
        public float4 _BlitScaleBias;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float2 uv;
    }

    [Uniform(0)] public static BlitData blit;

    public static Varyings FullScreen(uint id)
    {
        Varyings o;
        o.position = Sampling.GetFullScreenTriangleVertexPosition(id);
        o.uv = Sampling.GetFullScreenTriangleTexCoord(id) * blit._BlitScaleBias.xy + blit._BlitScaleBias.zw;
        return o;
    }

    /// <summary>A camera color as a linear one (fast: the polynomial curve).</summary>
    public static float3 ToLinear(float3 c, bool fast) => fast ? ColorGrading.FastSRGBToLinear(c) : LibColor.SRGBToLinear(Max(c, new float3(0.0f)));

    /// <summary>A linear color as the camera's.</summary>
    public static float3 ToGamma(float3 c, bool fast) => fast ? ColorGrading.FastLinearToSRGB(c) : LibColor.LinearToSRGB(Max(c, new float3(0.0f)));

    /// <summary>
    /// ColorAdjustments on a linear color: contrast in log space around middle gray, the color filter, the hue shift
    /// and the saturation in gamma space (hueSatCon: hue shift / 360, saturation and contrast as factors).
    /// </summary>
    public static float3 ColorGrade(float3 colorLinear, float4 hueSatCon, float3 colorFilter)
    {
        float3 colorLog = ColorGrading.LinearToLogC(colorLinear);
        colorLog = (colorLog - new float3(ColorGrading.ACEScc_MIDGRAY)) * hueSatCon.z + new float3(ColorGrading.ACEScc_MIDGRAY);
        colorLinear = ColorGrading.LogCToLinear(colorLog) * colorFilter;
        colorLinear = Max(colorLinear, new float3(0.0f));
        float3 colorGamma = Common.PositivePow(colorLinear, new float3(1.0f / 2.2f));
        float3 hsv = LibColor.RgbToHsv(colorGamma);
        hsv.x = ColorGrading.RotateHue(hsv.x + hueSatCon.x, 0.0f, 1.0f);
        colorGamma = LibColor.HsvToRgb(hsv);
        float luma = LibColor.Luminance(colorGamma);
        colorGamma = new float3(luma) + (colorGamma - new float3(luma)) * hueSatCon.y;
        return Common.PositivePow(Max(colorGamma, new float3(0.0f)), new float3(2.2f));
    }

    public static float3 Tonemap(float3 colorLinear, bool aces, bool neutral)
    {
        if (aces)
            return ACES.AcesTonemap(ACES.UnityToACES(colorLinear));
        if (neutral)
            return ColorGrading.NeutralTonemap(colorLinear);
        return colorLinear;
    }
}

/// <summary>
/// Bloom's passes: the prefilter (the bright part above the threshold with a soft knee, clamped; 13 taps with high
/// quality filtering), the 9 tap Gaussian blurs (horizontal into half the size, vertical at the size) and the upsample
/// that lerps the lower mip in by scatter (bicubic with high quality filtering). Linear colors throughout.
/// </summary>
[Shader("Hidden/Universal Render Pipeline/Bloom")]
public sealed class BloomShader : ShaderDefinition
{
    [Keyword(KeywordScope.Local)] public static readonly bool _BLOOM_HQ;
    [Keyword(KeywordScope.Local)] public static readonly bool _USE_FAST_SRGB_LINEAR_CONVERSION;

    public struct BloomData
    {
        /// <summary>x scatter, y clamp, z threshold (linear), w threshold knee.</summary>
        public float4 _Params;
    }

    [Uniform(1)] public static BloomData bloom;
    [Texture(0)] public static Texture2D<float4> _BlitTexture;
    [Sampler(0)] public static SamplerState sampler_BlitTexture;
    [Texture(1)] public static Texture2D<float4> _SourceTexLowMip;
    [Sampler(1)] public static SamplerState sampler_SourceTexLowMip;

    static float3 Source(float2 uv) => _BlitTexture.SampleLevel(sampler_BlitTexture, uv, 0.0f).xyz;

    static float2 TexelSize()
    {
        int2 size = _BlitTexture.Size;
        return new float2(1.0f / (float)size.x, 1.0f / (float)size.y);
    }

    [Pass("Bloom Prefilter", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class Prefilter
    {
        [VertexShader]
        public static PostProcessingCommon.Varyings Vertex([VertexId] uint id) => PostProcessingCommon.FullScreen(id);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(PostProcessingCommon.Varyings input)
        {
            float2 uv = input.uv;
            float3 color;
            if (_BLOOM_HQ)
            {
                float2 t = TexelSize();
                float3 A = Source(uv + t * new float2(-1.0f, -1.0f)), B = Source(uv + t * new float2(0.0f, -1.0f)), C = Source(uv + t * new float2(1.0f, -1.0f));
                float3 D = Source(uv + t * new float2(-0.5f, -0.5f)), E = Source(uv + t * new float2(0.5f, -0.5f));
                float3 F = Source(uv + t * new float2(-1.0f, 0.0f)), G = Source(uv), H = Source(uv + t * new float2(1.0f, 0.0f));
                float3 I = Source(uv + t * new float2(-0.5f, 0.5f)), J = Source(uv + t * new float2(0.5f, 0.5f));
                float3 K = Source(uv + t * new float2(-1.0f, 1.0f)), L = Source(uv + t * new float2(0.0f, 1.0f)), M = Source(uv + t * new float2(1.0f, 1.0f));
                float2 div = new float2(0.5f, 0.125f) * 0.25f;
                color = (D + E + I + J) * div.x;
                color += (A + B + G + F) * div.y;
                color += (B + C + H + G) * div.y;
                color += (F + G + L + K) * div.y;
                color += (G + H + M + L) * div.y;
            }
            else
                color = Source(uv);
            color = PostProcessingCommon.ToLinear(color, _USE_FAST_SRGB_LINEAR_CONVERSION);
            float4 p = bloom._Params;
            color = Min(color, new float3(p.y));
            float brightness = Common.Max3(color.x, color.y, color.z);
            float softness = Clamp(brightness - p.z + p.w, 0.0f, 2.0f * p.w);
            softness = softness * softness / (4.0f * p.w + 1e-4f);
            float multiplier = Max(brightness - p.z, softness) / Max(brightness, 1e-4f);
            return new float4(Max(color * multiplier, new float3(0.0f)), 1.0f);
        }
    }

    [Pass("Bloom Blur Horizontal", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class BlurHorizontal
    {
        [VertexShader]
        public static PostProcessingCommon.Varyings Vertex([VertexId] uint id) => PostProcessingCommon.FullScreen(id);

        /// <summary>Nine taps two source texels apart (the target is half the source's size).</summary>
        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(PostProcessingCommon.Varyings input)
        {
            float t = TexelSize().x * 2.0f;
            float2 uv = input.uv;
            float3 color = Source(uv - new float2(t * 4.0f, 0.0f)) * 0.01621622f;
            color += Source(uv - new float2(t * 3.0f, 0.0f)) * 0.05405405f;
            color += Source(uv - new float2(t * 2.0f, 0.0f)) * 0.12162162f;
            color += Source(uv - new float2(t, 0.0f)) * 0.19459459f;
            color += Source(uv) * 0.22702703f;
            color += Source(uv + new float2(t, 0.0f)) * 0.19459459f;
            color += Source(uv + new float2(t * 2.0f, 0.0f)) * 0.12162162f;
            color += Source(uv + new float2(t * 3.0f, 0.0f)) * 0.05405405f;
            color += Source(uv + new float2(t * 4.0f, 0.0f)) * 0.01621622f;
            return new float4(color, 1.0f);
        }
    }

    [Pass("Bloom Blur Vertical", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class BlurVertical
    {
        [VertexShader]
        public static PostProcessingCommon.Varyings Vertex([VertexId] uint id) => PostProcessingCommon.FullScreen(id);

        /// <summary>The same Gaussian in five bilinear taps.</summary>
        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(PostProcessingCommon.Varyings input)
        {
            float t = TexelSize().y;
            float2 uv = input.uv;
            float3 color = Source(uv - new float2(0.0f, t * 3.23076923f)) * 0.07027027f;
            color += Source(uv - new float2(0.0f, t * 1.38461538f)) * 0.31621622f;
            color += Source(uv) * 0.22702703f;
            color += Source(uv + new float2(0.0f, t * 1.38461538f)) * 0.31621622f;
            color += Source(uv + new float2(0.0f, t * 3.23076923f)) * 0.07027027f;
            return new float4(color, 1.0f);
        }
    }

    [Pass("Bloom Upsample", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class Upsample
    {
        [VertexShader]
        public static PostProcessingCommon.Varyings Vertex([VertexId] uint id) => PostProcessingCommon.FullScreen(id);

        static float3 Low(float2 uv) => _SourceTexLowMip.SampleLevel(sampler_SourceTexLowMip, uv, 0.0f).xyz;

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(PostProcessingCommon.Varyings input)
        {
            float3 high = Source(input.uv);
            float3 low;
            if (_BLOOM_HQ)
            {
                int2 lowSize = _SourceTexLowMip.Size;
                float2 size = new float2((float)lowSize.x, (float)lowSize.y);
                float4 p = Filtering.BicubicPositions(input.uv, size), w = Filtering.BicubicWeights(input.uv, size);
                low = Low(p.xy) * (w.x * w.y) + Low(new float2(p.z, p.y)) * (w.z * w.y) + Low(new float2(p.x, p.w)) * (w.x * w.w) + Low(p.zw) * (w.z * w.w);
            }
            else
                low = Low(input.uv);
            return new float4(Lerp(high, low, new float3(bloom._Params.x)), 1.0f);
        }
    }
}

/// <summary>The look up table parameters of the builders.</summary>
public struct LutBuilderData
{
    /// <summary>x lutHeight, y 0.5 / lutWidth, z 0.5 / lutHeight, w lutHeight / (lutHeight - 1).</summary>
    public float4 _Lut_Params;
    /// <summary>The color filter (linear).</summary>
    public float4 _ColorFilter;
    /// <summary>x hue shift / 360, y saturation, z contrast (factors).</summary>
    public float4 _HueSatCon;
}

/// <summary>The look up table of LDR grading: an sRGB coded color in, its grade out (sRGB coded).</summary>
[Shader("Hidden/Universal Render Pipeline/LutBuilderLdr")]
public sealed class LutBuilderLdr : ShaderDefinition
{
    [Uniform(1)] public static LutBuilderData lut;

    [Pass("LutBuilderLdr", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class Build
    {
        [VertexShader]
        public static PostProcessingCommon.Varyings Vertex([VertexId] uint id) => PostProcessingCommon.FullScreen(id);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(PostProcessingCommon.Varyings input)
        {
            float3 colorGamma = ColorGrading.GetLutStripValue(input.uv, lut._Lut_Params);
            float3 graded = PostProcessingCommon.ColorGrade(LibColor.SRGBToLinear(colorGamma), lut._HueSatCon, lut._ColorFilter.xyz);
            return new float4(LibColor.LinearToSRGB(Saturate(graded)), 1.0f);
        }
    }
}

/// <summary>The look up table of HDR grading: a LogC coded color in, its grade and tonemapping out (linear).</summary>
[Shader("Hidden/Universal Render Pipeline/LutBuilderHdr")]
public sealed class LutBuilderHdr : ShaderDefinition
{
    [Keyword(KeywordScope.Local)] public static readonly bool _TONEMAP_ACES;
    [Keyword(KeywordScope.Local)] public static readonly bool _TONEMAP_NEUTRAL;

    [Uniform(1)] public static LutBuilderData lut;

    [Pass("LutBuilderHdr", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class Build
    {
        [VertexShader]
        public static PostProcessingCommon.Varyings Vertex([VertexId] uint id) => PostProcessingCommon.FullScreen(id);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(PostProcessingCommon.Varyings input)
        {
            float3 colorLog = ColorGrading.GetLutStripValue(input.uv, lut._Lut_Params);
            float3 graded = PostProcessingCommon.ColorGrade(ColorGrading.LogCToLinear(colorLog), lut._HueSatCon, lut._ColorFilter.xyz);
            return new float4(PostProcessingCommon.Tonemap(graded, _TONEMAP_ACES, _TONEMAP_NEUTRAL), 1.0f);
        }
    }
}

/// <summary>
/// The final post processing pass: the camera's color in linear, bloom added by intensity and tint (and the dirt
/// texture over the screen), post exposure, the grading look up table (HDR: in LogC with the tonemapper baked in; LDR:
/// the tonemapper first, the table in sRGB), back to the camera's colors.
/// </summary>
[Shader("Hidden/Universal Render Pipeline/UberPost")]
public sealed class UberPost : ShaderDefinition
{
    [Keyword(KeywordScope.Local)] public static readonly bool _BLOOM_LQ;
    [Keyword(KeywordScope.Local)] public static readonly bool _BLOOM_HQ;
    [Keyword(KeywordScope.Local)] public static readonly bool _BLOOM_LQ_DIRT;
    [Keyword(KeywordScope.Local)] public static readonly bool _BLOOM_HQ_DIRT;
    [Keyword(KeywordScope.Local)] public static readonly bool _HDR_GRADING;
    [Keyword(KeywordScope.Local)] public static readonly bool _TONEMAP_ACES;
    [Keyword(KeywordScope.Local)] public static readonly bool _TONEMAP_NEUTRAL;
    [Keyword(KeywordScope.Local)] public static readonly bool _USE_FAST_SRGB_LINEAR_CONVERSION;

    public struct UberData
    {
        /// <summary>x 1 / lutWidth, y 1 / lutHeight, z lutHeight - 1, w the post exposure factor.</summary>
        public float4 _Lut_Params;
        /// <summary>x intensity, yzw the tint (linear, luminance 1).</summary>
        public float4 _Bloom_Params;
        /// <summary>The dirt texture's scale (xy) and offset (zw) over the screen.</summary>
        public float4 _LensDirt_Params;
        public float _LensDirt_Intensity;
    }

    [Uniform(1)] public static UberData uber;
    [Texture(0)] public static Texture2D<float4> _BlitTexture;
    [Sampler(0)] public static SamplerState sampler_BlitTexture;
    [Texture(1)] public static Texture2D<float4> _Bloom_Texture;
    [Sampler(1)] public static SamplerState sampler_Bloom_Texture;
    [Texture(2)] public static Texture2D<float4> _LensDirt_Texture;
    [Sampler(2)] public static SamplerState sampler_LensDirt_Texture;
    [Texture(3)] public static Texture2D<float4> _InternalLut;
    [Sampler(3)] public static SamplerState sampler_InternalLut;

    static float3 Bloom(float2 uv) => _Bloom_Texture.SampleLevel(sampler_Bloom_Texture, uv, 0.0f).xyz;

    static float3 Lut(float3 uvw)
    {
        float3 scaleOffset = uber._Lut_Params.xyz;
        float4 uv = ColorGrading.LutCoordinates(uvw, scaleOffset);
        float3 a = _InternalLut.SampleLevel(sampler_InternalLut, uv.xy, 0.0f).xyz, b = _InternalLut.SampleLevel(sampler_InternalLut, uv.zw, 0.0f).xyz;
        return Lerp(a, b, new float3(ColorGrading.LutBlend(uvw, scaleOffset)));
    }

    [Pass("UberPost", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class Uber
    {
        [VertexShader]
        public static PostProcessingCommon.Varyings Vertex([VertexId] uint id) => PostProcessingCommon.FullScreen(id);

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(PostProcessingCommon.Varyings input)
        {
            float2 uv = input.uv;
            float4 source = _BlitTexture.SampleLevel(sampler_BlitTexture, uv, 0.0f);
            float3 color = PostProcessingCommon.ToLinear(source.xyz, _USE_FAST_SRGB_LINEAR_CONVERSION);
            bool hq = _BLOOM_HQ || _BLOOM_HQ_DIRT;
            bool dirt = _BLOOM_LQ_DIRT || _BLOOM_HQ_DIRT;
            if (_BLOOM_LQ || hq || dirt)
            {
                float3 bloom;
                if (hq)
                {
                    int2 bloomSize = _Bloom_Texture.Size;
                    float2 size = new float2((float)bloomSize.x, (float)bloomSize.y);
                    float4 p = Filtering.BicubicPositions(uv, size), w = Filtering.BicubicWeights(uv, size);
                    bloom = Bloom(p.xy) * (w.x * w.y) + Bloom(new float2(p.z, p.y)) * (w.z * w.y) + Bloom(new float2(p.x, p.w)) * (w.x * w.w) + Bloom(p.zw) * (w.z * w.w);
                }
                else
                    bloom = Bloom(uv);
                bloom *= uber._Bloom_Params.x;
                color += bloom * uber._Bloom_Params.yzw;
                if (dirt)
                {
                    float3 lensDirt = _LensDirt_Texture.SampleLevel(sampler_LensDirt_Texture, uv * uber._LensDirt_Params.xy + uber._LensDirt_Params.zw, 0.0f).xyz;
                    color += lensDirt * uber._LensDirt_Intensity * bloom;
                }
            }
            color *= uber._Lut_Params.w;
            float3 gamma;
            if (_HDR_GRADING)
                gamma = PostProcessingCommon.ToGamma(Lut(Saturate(ColorGrading.LinearToLogC(color))), _USE_FAST_SRGB_LINEAR_CONVERSION);
            else
            {
                color = PostProcessingCommon.Tonemap(color, _TONEMAP_ACES, _TONEMAP_NEUTRAL);
                gamma = Lut(LibColor.LinearToSRGB(Saturate(color)));
            }
            return new float4(gamma, source.w);
        }
    }
}
