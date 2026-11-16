using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace Helix.RenderPipelines.Standard.TestShaders;

[System.Runtime.CompilerServices.InlineArray(32)]
public struct LightVectors
{
    private float4 element;
}

[System.Runtime.CompilerServices.InlineArray(5)]
public struct ShadowMatrices
{
    private float4x4 element;
}

/// <summary>The pipeline's lights and the main light's shadow as the shaders read them (globals by name).</summary>
public struct PipelineLights
{
    public float4 _MainLightPosition;
    public float4 _MainLightColor;
    public float4 _AdditionalLightsCount;
    public LightVectors _AdditionalLightsPosition;
    public LightVectors _AdditionalLightsColor;
    public LightVectors _AdditionalLightsAttenuation;
    public ShadowMatrices _MainLightWorldToShadow;
    public float4 _MainLightShadowParams;
}

/// <summary>
/// A lit surface of the pipeline's forward pass: the main light (with its shadow from the first cascade) and the
/// object's additional point lights (the range's smooth fade), plus a little ambient; a shadow caster pass.
/// </summary>
[Shader("Tests/StandardLit")]
public sealed class StandardLit : ShaderDefinition
{
    [Property("Base Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 _BaseColor;

    public struct MaterialData
    {
        public float4 _BaseColor;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float3 normal;
        [Location(1)] public float3 world;
    }

    [Pass("ForwardLit", LightMode = "UniversalForward")]
    public static class Forward
    {
        [Uniform(0)] public static MaterialData material;
        [Uniform(1)] public static PipelineLights lights;
        [Texture(0)] public static Texture2D<float> _MainLightShadowmapTexture;
        [Sampler(0)] public static SamplerComparisonState shadowSampler;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Normal)] float3 normal)
        {
            Varyings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.normal = ShaderVariables.ObjectToWorldNormal(normal);
            o.world = ShaderVariables.ObjectToWorld(position);
            return o;
        }

        static float MainShadow(float3 world)
        {
            if (lights._MainLightShadowParams.x <= 0.0f)
                return 1.0f;
            float4 c = Mul(lights._MainLightWorldToShadow[0], new float4(world, 1.0f));
            float3 coord = c.xyz / c.w;
            if (coord.x < 0.0f || coord.x > 1.0f || coord.y < 0.0f || coord.y > 1.0f)
                return 1.0f;
            return _MainLightShadowmapTexture.SampleCmpLevelZero(shadowSampler, coord.xy, coord.z);
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float3 n = Normalize(input.normal);
            float main = Saturate(Dot(n, lights._MainLightPosition.xyz)) * MainShadow(input.world);
            float3 light = lights._MainLightColor.xyz * main + new float3(0.1f);
            int count = ShaderVariables.ObjectLightCount();
            for (int i = 0; i < 8; i++)
            {
                if (i >= count)
                    break;
                int index = ShaderVariables.ObjectLightIndex(i);
                float4 p = lights._AdditionalLightsPosition[index];
                float3 toLight = p.xyz - input.world * p.w;
                float distanceSqr = Max(Dot(toLight, toLight), 0.0001f);
                float4 attenuation = lights._AdditionalLightsAttenuation[index];
                float factor = distanceSqr * attenuation.x;
                float smooth = Saturate(1.0f - factor * factor);
                float fade = smooth * smooth / distanceSqr;
                light += lights._AdditionalLightsColor[index].xyz * (fade * Saturate(Dot(n, toLight * Rsqrt(distanceSqr))));
            }
            return new float4(material._BaseColor.xyz * light, 1.0f);
        }
    }

    [Pass("ShadowCaster", LightMode = "ShadowCaster")]
    public static class ShadowCaster
    {
        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position) => new Varyings { position = ShaderVariables.ObjectToClip(position) };

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input) => new float4(0.0f);
    }

    [Pass("DepthOnly", LightMode = "DepthOnly", ColorMask = ColorWriteMask.None)]
    public static class DepthOnly
    {
        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position) => new Varyings { position = ShaderVariables.ObjectToClip(position) };

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input) => new float4(0.0f);
    }
}

/// <summary>
/// The pipeline's copies read back: one half of the pane shows _CameraOpaqueTexture at the pixel's screen position,
/// the other _CameraDepthTexture as gray, nearer brighter (drawn in the transparent queue, after both copies).
/// </summary>
[Shader("Tests/StandardReadCopies")]
[Tag("Queue", "Transparent")]
public sealed class StandardReadCopies : ShaderDefinition
{
    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float4 clip;
        [Location(1)] public float2 uv;
    }

    [Pass("ReadCopies", LightMode = "UniversalForward", ZWrite = false)]
    public static class Forward
    {
        [Texture(0)] public static Texture2D<float4> _CameraOpaqueTexture;
        [Texture(1)] public static Texture2D<float> _CameraDepthTexture;
        [Sampler(0)] public static SamplerState opaqueSampler;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.TexCoord0)] float2 uv)
        {
            Varyings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.clip = o.position;
            o.uv = uv;
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            // The pixel's place in the camera's textures (a flipped target's projection turned back).
            float2 ndc = input.clip.xy / input.clip.w;
            float2 screen = new float2(ndc.x, ndc.y * ShaderVariables.camera.helix_ProjectionParams.x) * 0.5f + new float2(0.5f);
            if (input.uv.x < 0.5f)
                return new float4(_CameraOpaqueTexture.SampleLevel(opaqueSampler, screen, 0.0f).xyz, 1.0f);
            int2 size = _CameraDepthTexture.Size;
            int2 texel = new int2(Clamp((int)(screen.x * (float)size.x), 0, size.x - 1), Clamp((int)(screen.y * (float)size.y), 0, size.y - 1));
            float d = _CameraDepthTexture.Load(texel, 0);
            return new float4(new float3(Saturate((1.0f - d) * 20.0f)), 1.0f);
        }
    }
}

/// <summary>A color of the unlit pass every pipeline draws.</summary>
[Shader("Tests/StandardUnlit")]
public sealed class StandardUnlit : ShaderDefinition
{
    [Property("Base Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 _BaseColor;

    public struct MaterialData
    {
        public float4 _BaseColor;
    }

    public struct Varyings
    {
        [Position] public float4 position;
    }

    [Pass("Unlit")]
    public static class Forward
    {
        [Uniform(0)] public static MaterialData material;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position) => new Varyings { position = ShaderVariables.ObjectToClip(position) };

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input) => material._BaseColor;
    }
}

/// <summary>A blended color in the transparent queue.</summary>
[Shader("Tests/StandardTransparent")]
[Tag("Queue", "Transparent")]
public sealed class StandardTransparent : ShaderDefinition
{
    [Property("Base Color"), MainColor, Default(1.0f, 1.0f, 1.0f, 0.5f)] public float4 _BaseColor;

    public struct MaterialData
    {
        public float4 _BaseColor;
    }

    public struct Varyings
    {
        [Position] public float4 position;
    }

    [Pass("ForwardTransparent", LightMode = "UniversalForward", ZWrite = false)]
    [Blend(BlendFactor.SrcAlpha, BlendFactor.OneMinusSrcAlpha)]
    public static class Forward
    {
        [Uniform(0)] public static MaterialData material;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position) => new Varyings { position = ShaderVariables.ObjectToClip(position) };

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input) => material._BaseColor;
    }
}

/// <summary>The Blitter's source times a global tint over the whole target (the C# test feature's pass).</summary>
[Shader("Tests/StandardTint")]
public sealed class StandardTint : ShaderDefinition
{
    public struct BlitData
    {
        public float4 _BlitScaleBias;
    }

    public struct TintData
    {
        public float4 _TestTint;
    }

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float2 uv;
    }

    [Uniform(0)] public static BlitData blit;
    [Uniform(1)] public static TintData tint;
    [Texture(0)] public static Texture2D<float4> _BlitTexture;
    [Sampler(0)] public static SamplerState sampler_BlitTexture;

    [Pass("Tint", Cull = CullMode.Off, ZTest = CompareFunction.Always, ZWrite = false)]
    public static class Tint
    {
        [VertexShader]
        public static Varyings Vertex([VertexId] uint id)
        {
            float2 uv = new float2((float)((id << 1) & 2u), (float)(id & 2u));
            Varyings o;
            o.position = new float4(uv * 2.0f - new float2(1.0f), 0.5f, 1.0f);
            o.position.y *= ShaderVariables.camera.helix_ProjectionParams.x;
            o.uv = uv * blit._BlitScaleBias.xy + blit._BlitScaleBias.zw;
            return o;
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input) => _BlitTexture.SampleLevel(sampler_BlitTexture, input.uv, 0.0f) * tint._TestTint;
    }
}
