using System.Collections.Generic;

namespace HelixEngine.Rendering.Universal
{
    // The C# members of RenderingUtils: drawing settings of the frame and the reallocation of handles.
    public static partial class RenderingUtils
    {
        /// <summary>Drawing settings of a pass of a light mode for the frame (the camera's sorting, the pipeline's per
        /// object data, the main light).</summary>
        public static DrawingSettings CreateDrawingSettings(ShaderTagId shaderTagId, UniversalRenderingData renderingData, UniversalCameraData cameraData,
                                                           UniversalLightData lightData, SortingCriteria sortingCriteria) =>
            ScriptableRenderPass.CreateDrawingSettings(shaderTagId, renderingData, cameraData, lightData, sortingCriteria);

        public static DrawingSettings CreateDrawingSettings(List<ShaderTagId> shaderTagIdList, UniversalRenderingData renderingData, UniversalCameraData cameraData,
                                                           UniversalLightData lightData, SortingCriteria sortingCriteria) =>
            ScriptableRenderPass.CreateDrawingSettings(shaderTagIdList, renderingData, cameraData, lightData, sortingCriteria);

        /// <summary>The view and projection of the draws after it, with the inverse ones when asked.</summary>
        public static void SetViewAndProjectionMatrices(CommandBuffer cmd, Matrix4x4 viewMatrix, Matrix4x4 projectionMatrix, bool setInverseMatrices) =>
            SetViewAndProjectionMatricesNative(global::Helix.Interop.RenderingInterop.IdOf(cmd), viewMatrix, projectionMatrix, setInverseMatrices);

        public static void SetViewAndProjectionMatrices(RasterCommandBuffer cmd, Matrix4x4 viewMatrix, Matrix4x4 projectionMatrix, bool setInverseMatrices) =>
            SetViewAndProjectionMatrices(global::Helix.Interop.RenderGraphInterop.CommandBufferOf(cmd), viewMatrix, projectionMatrix, setInverseMatrices);

        /// <summary>A new handle like the descriptor when the handle is null or differs from it (the old one released);
        /// true when it was made.</summary>
        public static bool ReAllocateHandleIfNeeded(ref RTHandle handle, in RenderTextureDescriptor descriptor, FilterMode filterMode = FilterMode.Point,
                                                    TextureWrapMode wrapMode = TextureWrapMode.Repeat, bool isShadowMap = false, int anisoLevel = 1,
                                                    float mipMapBias = 0, string name = "")
        {
            if (handle != null && handle.rt != null && Same(handle.rt.descriptor, descriptor) && handle.rt.filterMode == filterMode && handle.rt.wrapMode == wrapMode &&
                handle.name == name)
                return false;
            handle?.Release();
            RenderTextureDescriptor d = descriptor;
            if (isShadowMap)
                d.shadowSamplingMode = ShadowSamplingMode.CompareDepths;
            handle = RTHandles.Alloc(d, filterMode, wrapMode, isShadowMap, anisoLevel, mipMapBias, name);
            return true;
        }

        static bool Same(RenderTextureDescriptor a, RenderTextureDescriptor b) =>
            a.width == b.width && a.height == b.height && a.graphicsFormat == b.graphicsFormat && a.depthStencilFormat == b.depthStencilFormat &&
            a.msaaSamples == b.msaaSamples && a.dimension == b.dimension && a.volumeDepth == b.volumeDepth && a.useMipMap == b.useMipMap &&
            a.enableRandomWrite == b.enableRandomWrite;
    }
}
