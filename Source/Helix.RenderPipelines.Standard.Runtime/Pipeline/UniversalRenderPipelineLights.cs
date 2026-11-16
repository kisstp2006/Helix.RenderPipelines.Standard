using System;
using Helix.Collections;
using Helix.Collections.LowLevel.Unsafe;

namespace HelixEngine.Rendering.Universal
{
    // The pipeline's light helpers in C# (the native pipeline's own).
    public sealed partial class UniversalRenderPipeline
    {
        /// <summary>Whether the camera is a game camera (not a scene view, preview or reflection camera).</summary>
        public static bool IsGameCamera(Camera camera)
        {
            if (camera == null)
                throw new ArgumentNullException(nameof(camera));
            return UniversalRenderPipelineNative.IsGameCamera(camera);
        }

        /// <summary>A light's attenuation (range and spot cone) and spot direction as the shaders read them.</summary>
        public static void GetLightAttenuationAndSpotDirection(LightType lightType, float lightRange, Matrix4x4 lightLocalToWorldMatrix, float spotAngle,
                                                               float? innerSpotAngle, out Vector4 lightAttenuation, out Vector4 lightSpotDir) =>
            UniversalRenderPipelineNative.LightAttenuation(lightType, lightRange, lightLocalToWorldMatrix, spotAngle, innerSpotAngle.HasValue, innerSpotAngle ?? 0.0f,
                                                           out lightAttenuation, out lightSpotDir);

        /// <summary>A visible light's position (or direction), color, attenuation, spot direction and occlusion probe
        /// channel as the shaders read them.</summary>
        public static unsafe void InitializeLightConstants_Common(NativeArray<VisibleLight> lights, int lightIndex, out Vector4 lightPos, out Vector4 lightColor,
                                                                  out Vector4 lightAttenuation, out Vector4 lightSpotDir, out Vector4 lightOcclusionProbeChannel) =>
            UniversalRenderPipelineNative.LightConstants((ulong)NativeArrayUnsafeUtility.GetUnsafeReadOnlyPtr(lights), lights.Length, lightIndex, out lightPos,
                                                         out lightColor, out lightAttenuation, out lightSpotDir, out lightOcclusionProbeChannel);
    }
}
