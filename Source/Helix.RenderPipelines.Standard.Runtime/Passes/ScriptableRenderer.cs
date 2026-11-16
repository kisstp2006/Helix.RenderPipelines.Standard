using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using HelixEngine.Rendering.RenderGraphModule;

namespace HelixEngine.Rendering.Universal
{
    // The C# side of renderers: C# classes below ScriptableRenderer (made by a C# renderer data's Create) override
    // the bound virtual functions and SetupCullingParameters, which the native pipeline reaches through the function
    // set here.
    public partial class ScriptableRenderer : IDisposable
    {
        static unsafe ScriptableRenderer() =>
            SetManaged((ulong)(delegate* unmanaged[Cdecl]<ulong, ulong, ulong, byte>)&CullManaged, (ulong)(delegate* unmanaged[Cdecl]<ulong, int, byte>)&DeviceManaged);

        /// <summary>The device types the renderer does not draw on (a camera that uses it renders nothing there).</summary>
        public virtual GraphicsDeviceType[] unsupportedGraphicsDeviceTypes => UnsupportedGraphicsDeviceTypesNative();

        /// <summary>The culling of a camera: a renderer changes the camera's parameters (the forward renderer sets the
        /// shadow distance).</summary>
        public virtual unsafe void SetupCullingParameters(ref ScriptableCullingParameters cullingParameters, ref CameraData cameraData)
        {
            ulong frame = cameraData.frameData != null ? global::Helix.Interop.RenderGraphInterop.IdOf(cameraData.frameData) : 0;
            byte* native = stackalloc byte[CullingSize];
            ulong address = (ulong)native;
            global::Helix.Interop.RenderingInterop.WriteCullingParametersAt(address, cullingParameters);
            // A C# renderer's base call runs ScriptableRenderer's own; a C++ renderer's its override.
            if (((global::Helix.Interop.INativeObject)this).HxDerived)
                BaseSetupCullingParameters(address, frame);
            else
                SetupCullingParametersNative(address, frame);
            cullingParameters = global::Helix.Interop.RenderingInterop.CullingParametersAt(address);
        }

        static int s_CullingSize;

        static int CullingSize => s_CullingSize != 0 ? s_CullingSize : s_CullingSize = global::Helix.Interop.RenderingInterop.CullingParametersSize;

        /// <summary>The camera's matrices as the shaders read them, with the inverse ones when asked.</summary>
        public static void SetCameraMatrices(CommandBuffer cmd, UniversalCameraData cameraData, bool setInverseMatrices) =>
            SetCameraMatricesNative(global::Helix.Interop.RenderingInterop.IdOf(cmd), cameraData, setInverseMatrices);

        public static void SetCameraMatrices(RasterCommandBuffer cmd, UniversalCameraData cameraData, bool setInverseMatrices) =>
            SetCameraMatrices(global::Helix.Interop.RenderGraphInterop.CommandBufferOf(cmd), cameraData, setInverseMatrices);

        public static void SetCameraMatrices(CommandBuffer cmd, ref CameraData cameraData, bool setInverseMatrices) =>
            SetCameraMatrices(cmd, cameraData.frameData.Get<UniversalCameraData>(), setInverseMatrices);

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static byte DeviceManaged(ulong wrapper, int device)
        {
            try
            {
                if (GCHandle.FromIntPtr((IntPtr)wrapper).Target is not ScriptableRenderer renderer || !((global::Helix.Interop.INativeObject)renderer).HxDerived)
                    return 2;
                GraphicsDeviceType[] unsupported = renderer.unsupportedGraphicsDeviceTypes;
                return unsupported != null && Array.IndexOf(unsupported, (GraphicsDeviceType)device) >= 0 ? (byte)0 : (byte)1;
            }
            catch (Exception e)
            {
                Debug.LogException(e);
                return 1;
            }
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static byte CullManaged(ulong wrapper, ulong parameters, ulong frame)
        {
            try
            {
                if (GCHandle.FromIntPtr((IntPtr)wrapper).Target is not ScriptableRenderer renderer || !((global::Helix.Interop.INativeObject)renderer).HxDerived)
                    return 0;
                ScriptableCullingParameters p = global::Helix.Interop.RenderingInterop.CullingParametersAt(parameters);
                var cameraData = new CameraData(global::Helix.Interop.RenderGraphInterop.ContainerOf(frame));
                renderer.SetupCullingParameters(ref p, ref cameraData);
                global::Helix.Interop.RenderingInterop.WriteCullingParametersAt(parameters, p);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
            return 1;
        }
        /// <summary>The passes queued for the camera's frame (a copy: EnqueuePass adds to the queue).</summary>
        protected List<ScriptableRenderPass> activeRenderPassQueue => new List<ScriptableRenderPass>(activeRenderPassQueueArray);

        /// <summary>The active features of the renderer's data.</summary>
        protected List<ScriptableRendererFeature> rendererFeatures => new List<ScriptableRendererFeature>(rendererFeaturesArray);

        static ProfilingSampler s_Execute;

        /// <summary>The sampler around the renderer's frame.</summary>
        public static ProfilingSampler profilingExecute => s_Execute ??= new ProfilingSampler("ScriptableRenderer.Execute");

        public void Dispose()
        {
            DisposeNative();
            GC.SuppressFinalize(this);
        }
    }

    public partial class UniversalRenderPipelineAsset
    {
        /// <summary>The renderers of the renderer data (made on first use).</summary>
        public ReadOnlySpan<ScriptableRenderer> renderers => renderersArray;
    }

    public sealed partial class UniversalRenderer
    {
        /// <summary>A texture of the frame's graph like the descriptor (a depth texture when it has a depth format).</summary>
        public static unsafe TextureHandle CreateRenderGraphTexture(RenderGraph renderGraph, RenderTextureDescriptor desc, string name, bool clear,
                                                                    FilterMode filterMode = FilterMode.Point, TextureWrapMode wrapMode = TextureWrapMode.Clamp)
        {
            // The descriptor's plain form (13 ints) at a native address.
            int* data = stackalloc int[16];
            global::Helix.Interop.RenderingInterop.WriteDescriptorAt((ulong)data, desc);
            return global::Helix.Interop.RenderGraphInterop.TextureOf(CreateRenderGraphTextureNative(global::Helix.Interop.RenderGraphInterop.IdOf(renderGraph),
                                                                                                    (ulong)data, name, clear, filterMode, wrapMode));
        }
    }

    /// <summary>The frame as the features see it: a view over the frame data (the reference's compatibility struct).</summary>
    public struct RenderingData
    {
        internal ContextContainer frameData;

        internal RenderingData(ContextContainer frame) => frameData = frame;

        public CameraData cameraData => new CameraData(frameData);
        public CullingResults cullResults => frameData.Get<UniversalRenderingData>().cullResults;
        public bool supportsDynamicBatching => frameData.Get<UniversalRenderingData>().supportsDynamicBatching;
        public PerObjectData perObjectData => frameData.Get<UniversalRenderingData>().perObjectData;
        public bool postProcessingEnabled => frameData.Get<UniversalPostProcessingData>().isEnabled;
    }

    /// <summary>The camera of the frame as the features see it: a view over the frame data's UniversalCameraData.</summary>
    public struct CameraData
    {
        internal ContextContainer frameData;

        internal CameraData(ContextContainer frame) => frameData = frame;

        UniversalCameraData data => frameData.Get<UniversalCameraData>();

        public Camera camera => data.camera;
        public CameraType cameraType => data.cameraType;
        public CameraRenderType renderType => data.renderType;
        public RenderTexture targetTexture => data.targetTexture;
        public RenderTextureDescriptor cameraTargetDescriptor => data.cameraTargetDescriptor;
        public float renderScale => data.renderScale;
        public bool isSceneViewCamera => data.isSceneViewCamera;
        public bool isPreviewCamera => data.isPreviewCamera;
        public bool isHdrEnabled => data.isHdrEnabled;
        public bool requiresDepthTexture => data.requiresDepthTexture;
        public bool requiresOpaqueTexture => data.requiresOpaqueTexture;
        public bool postProcessEnabled => data.postProcessEnabled;
        public ScriptableRenderer renderer => data.renderer;
        public Vector3 worldSpaceCameraPos => data.worldSpaceCameraPos;
        public Matrix4x4 GetViewMatrix(int viewIndex = 0) => data.GetViewMatrix(viewIndex);
        public Matrix4x4 GetProjectionMatrix(int viewIndex = 0) => data.GetProjectionMatrix(viewIndex);
        public Matrix4x4 GetGPUProjectionMatrix(int viewIndex = 0) => data.GetGPUProjectionMatrix(viewIndex);
        public bool IsCameraProjectionMatrixFlipped() => data.IsCameraProjectionMatrixFlipped();
    }
}
