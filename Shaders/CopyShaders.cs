using HelixEngine.Rendering.ShaderLibrary;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>The camera's depth (the Blitter's _BlitTexture) into the bound depth texture: _CameraDepthTexture.</summary>
[Shader("Hidden/Universal Render Pipeline/CopyDepth")]
public sealed class CopyDepth : ShaderDefinition
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

    public struct DepthOnly
    {
        [Depth] public float depth;
    }

    [Uniform(0)] public static BlitData blit;
    [Texture(0)] public static Texture2D<float> _BlitTexture;

    [Pass("CopyDepth", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = true, ColorMask = ColorWriteMask.None)]
    public static class Copy
    {
        [VertexShader]
        public static Varyings Vertex([VertexId] uint id)
        {
            Varyings o;
            o.position = Sampling.GetFullScreenTriangleVertexPosition(id);
            o.uv = Sampling.GetFullScreenTriangleTexCoord(id) * blit._BlitScaleBias.xy + blit._BlitScaleBias.zw;
            return o;
        }

        [FragmentShader]
        public static DepthOnly Fragment(Varyings input)
        {
            int2 size = _BlitTexture.Size;
            int2 texel = new int2(Clamp((int)Floor(input.uv.x * (float)size.x), 0, size.x - 1), Clamp((int)Floor(input.uv.y * (float)size.y), 0, size.y - 1));
            DepthOnly o;
            o.depth = _BlitTexture.Load(texel, 0);
            return o;
        }
    }
}

/// <summary>The opaque texture's 4x box downsampling: four bilinear taps _SampleOffset texels around each texel.</summary>
[Shader("Hidden/Universal Render Pipeline/Sampling")]
public sealed class SamplingShader : ShaderDefinition
{
    [Property("Sample Offset"), Default(2.0f)] public float _SampleOffset;

    public struct MaterialData
    {
        public float _SampleOffset;
    }

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
    [Uniform(1)] public static MaterialData material;
    [Texture(0)] public static Texture2D<float4> _BlitTexture;
    [Sampler(0)] public static SamplerState blitSampler;

    [Pass("BoxDownsample", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class Box
    {
        [VertexShader]
        public static Varyings Vertex([VertexId] uint id)
        {
            Varyings o;
            o.position = Sampling.GetFullScreenTriangleVertexPosition(id);
            o.uv = Sampling.GetFullScreenTriangleTexCoord(id) * blit._BlitScaleBias.xy + blit._BlitScaleBias.zw;
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            int2 size = _BlitTexture.Size;
            float2 texel = new float2(1.0f / (float)size.x, 1.0f / (float)size.y);
            float4 d = new float4(-texel.x, -texel.y, texel.x, texel.y) * material._SampleOffset;
            float4 s = _BlitTexture.SampleLevel(blitSampler, input.uv + d.xy, 0.0f);
            s += _BlitTexture.SampleLevel(blitSampler, input.uv + d.zy, 0.0f);
            s += _BlitTexture.SampleLevel(blitSampler, input.uv + d.xw, 0.0f);
            s += _BlitTexture.SampleLevel(blitSampler, input.uv + d.zw, 0.0f);
            return s * 0.25f;
        }
    }
}
