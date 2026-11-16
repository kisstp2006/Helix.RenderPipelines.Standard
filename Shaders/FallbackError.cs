using HelixEngine.Builtin;
using HelixEngine.Shaders;

namespace HelixEngine.Rendering.Universal.Shaders;

/// <summary>What the pipeline draws an object with whose shader it cannot draw: magenta.</summary>
[Shader("Hidden/Universal Render Pipeline/FallbackError")]
public sealed class FallbackError : ShaderDefinition
{
    public struct Varyings
    {
        [Position] public float4 position;
    }

    [Pass("Error", LightMode = "UniversalForward")]
    public static class Error
    {
        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position) => new Varyings { position = ShaderVariables.ObjectToClip(position) };

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input) => new float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
}
