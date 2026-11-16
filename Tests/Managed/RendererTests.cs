using System.Collections.Generic;
using HelixEngine;
using HelixEngine.Experimental.Rendering;
using HelixEngine.Rendering;
using HelixEngine.Rendering.RenderGraphModule;
using HelixEngine.Rendering.Universal;

namespace Tests
{
    // A renderer written in C#: its data makes it, it queues its own pass when the frame begins, drops layer 3 from the
    // culling and draws the opaque and transparent objects itself.
    public class CsRendererData : ScriptableRendererData
    {
        // The device types its renderer does not draw on (set by the tests).
        public static GraphicsDeviceType[] unsupported = new GraphicsDeviceType[0];

        protected override ScriptableRenderer Create() => new CsRenderer(this);
    }

    public class CsRenderer : ScriptableRenderer
    {
        readonly CsDrawPass m_Pass = new CsDrawPass { renderPassEvent = RenderPassEvent.BeforeRenderingOpaques };

        public CsRenderer(ScriptableRendererData data) : base(data) { }

        public override GraphicsDeviceType[] unsupportedGraphicsDeviceTypes => CsRendererData.unsupported;

        public override void OnBeginRenderGraphFrame() => EnqueuePass(m_Pass);

        public override void SetupCullingParameters(ref ScriptableCullingParameters cullingParameters, ref CameraData cameraData)
        {
            base.SetupCullingParameters(ref cullingParameters, ref cameraData);
            cullingParameters.cullingMask &= ~(1u << 3);
            cullingParameters.shadowDistance = 0;
            Notes.Note($"cull:{cameraData.camera.name}:{cullingParameters.cullingMask:X}");
        }
    }

    // Draws the frame's objects into the camera's color and depth (through the C# helpers of the pipeline).
    public class CsDrawPass : ScriptableRenderPass
    {
        static readonly List<ShaderTagId> s_Names = new() { new ShaderTagId("UniversalForward"), new ShaderTagId("SRPDefaultUnlit") };

        class PassData
        {
            public RendererListHandle opaque, transparent;
            public UniversalCameraData camera;
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
        {
            UniversalCameraData camera = frameData.Get<UniversalCameraData>();
            UniversalRenderingData rendering = frameData.Get<UniversalRenderingData>();
            UniversalLightData lights = frameData.Get<UniversalLightData>();
            UniversalResourceData resources = frameData.Get<UniversalResourceData>();
            TextureHandle scratch = UniversalRenderer.CreateRenderGraphTexture(renderGraph, new RenderTextureDescriptor(8, 8, RenderTextureFormat.ARGB32, 0), "Scratch", true);
            Notes.Note($"draw:{camera.scaledWidth}:{scratch.IsValid()}:{UniversalRenderer.IsOffscreenDepthTexture(camera)}:" +
                       $"{RenderingUtils.SupportsRenderTextureFormat(RenderTextureFormat.ARGB32)}:{resources.isActiveTargetBackBuffer}");
            using (IRasterRenderGraphBuilder builder = renderGraph.AddRasterRenderPass("CsDraw", out PassData data))
            {
                DrawingSettings opaque = RenderingUtils.CreateDrawingSettings(s_Names, rendering, camera, lights, SortingCriteria.CommonOpaque);
                DrawingSettings transparent = RenderingUtils.CreateDrawingSettings(s_Names, rendering, camera, lights, SortingCriteria.CommonTransparent);
                var opaqueFilter = new FilteringSettings(RenderQueueRange.opaque);
                var transparentFilter = new FilteringSettings(RenderQueueRange.transparent);
                data.opaque = renderGraph.CreateRendererList(new RendererListParams(rendering.cullResults, opaque, opaqueFilter));
                data.transparent = renderGraph.CreateRendererList(new RendererListParams(rendering.cullResults, transparent, transparentFilter));
                data.camera = camera;
                builder.UseRendererList(data.opaque);
                builder.UseRendererList(data.transparent);
                builder.SetRenderAttachment(resources.activeColorTexture, 0);
                builder.SetRenderAttachmentDepth(resources.activeDepthTexture);
                builder.AllowGlobalStateModification(true);
                builder.SetRenderFunc((PassData d, RasterGraphContext ctx) =>
                {
                    ScriptableRenderer.SetCameraMatrices(ctx.cmd, d.camera, true);
                    ctx.cmd.DrawRendererList(d.opaque);
                    ctx.cmd.DrawRendererList(d.transparent);
                });
            }
        }
    }

    // A pass queued for one frame and dropped: its native object goes once it is collected.
    public class CsDroppedPass : ScriptableRenderPass
    {
        public static ulong lastId;

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData) => Notes.Note("dropped pass recorded");
    }

    public class CsDropFeature : ScriptableRendererFeature
    {
        public override void Create() { }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            var pass = new CsDroppedPass { renderPassEvent = RenderPassEvent.AfterRenderingOpaques };
            CsDroppedPass.lastId = ((Helix.Interop.INativeObject)pass).HxId;
            renderer.EnqueuePass(pass);
        }
    }

    // The objects collected (what C++ calls between its frames in the lifetime test).
    public class CsCollect : GameBehaviour
    {
        void Start()
        {
            System.GC.Collect();
            System.GC.WaitForPendingFinalizers();
            System.GC.Collect();
            Notes.Note($"collected:{CsDroppedPass.lastId}");
        }
    }

    // The C# renderer's unsupported device types: this device, then none again.
    public class CsUnsupportedDevice : GameBehaviour
    {
        void Start() => CsRendererData.unsupported = new[] { SystemInfo.graphicsDeviceType };
    }

    public class CsSupportedDevices : GameBehaviour
    {
        void Start() => CsRendererData.unsupported = new GraphicsDeviceType[0];
    }

    // The global settings the asset made before its pipeline, the C# pipeline as the current one.
    public class CsGlobalSettingsTests : GameBehaviour
    {
        void Start()
        {
            var settings = UniversalRenderPipelineGlobalSettings.instance;
            bool volume = GraphicsSettings.TryGetRenderPipelineSettings(out URPDefaultVolumeProfileSettings v);
            bool graph = GraphicsSettings.TryGetRenderPipelineSettings(out RenderGraphSettings g);
            RenderPipeline current = RenderPipelineManager.currentPipeline;
            Notes.Note($"settings:{settings != null}:{volume}:{v?.version}:{v?.volumeProfile == null}:{graph}:{g?.version}:" +
                       $"{current is UniversalRenderPipeline}:{current?.defaultSettings == settings}:{SystemInfo.graphicsDeviceType != GraphicsDeviceType.Null}");
        }
    }
}

namespace Tests
{
    // The skybox drawn by a C# pass through the C# graph's skybox list, after the opaques.
    public class CsSkyPass : ScriptableRenderPass
    {
        class PassData
        {
            public RendererListHandle sky;
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
        {
            UniversalCameraData camera = frameData.Get<UniversalCameraData>();
            UniversalResourceData resources = frameData.Get<UniversalResourceData>();
            using (IRasterRenderGraphBuilder builder = renderGraph.AddRasterRenderPass("CsSky", out PassData data))
            {
                Camera eye = camera.camera;
                data.sky = renderGraph.CreateSkyboxRendererList(in eye);
                builder.UseRendererList(data.sky);
                builder.SetRenderAttachment(resources.activeColorTexture, 0);
                builder.SetRenderAttachmentDepth(resources.activeDepthTexture);
                builder.SetRenderFunc((PassData d, RasterGraphContext ctx) => ctx.cmd.DrawRendererList(d.sky));
            }
        }
    }

    public class CsSkyFeature : ScriptableRendererFeature
    {
        CsSkyPass m_Pass;

        public override void Create() => m_Pass = new CsSkyPass { renderPassEvent = RenderPassEvent.AfterRenderingOpaques };

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData) => renderer.EnqueuePass(m_Pass);
    }
}

namespace Tests
{
    // The post processing volume components from C#: the global volume's profile, its components of the pipeline's C#
    // classes, their parameters (enum parameters of their own classes); the saturation turned all the way down.
    public class CsPostTweaks : GameBehaviour
    {
        void Start()
        {
            VolumeProfile profile = GameObject.Find("PostVolume").GetComponent<Volume>().sharedProfile;
            bool hasBloom = profile.TryGet(out Bloom bloom);
            bool hasTonemapping = profile.TryGet(out Tonemapping tonemapping);
            bool hasAdjustments = profile.TryGet(out ColorAdjustments adjustments);
            adjustments.saturation.Override(-100);
            var data = (UniversalRendererData)((UniversalRenderPipelineAsset)GraphicsSettings.defaultRenderPipeline).rendererDataList[0];
            PostProcessData.ShaderResources shaders = data.postProcessData.shaders;
            Notes.Note($"data:{shaders.bloomPS.name}:{shaders.uberPostPS.name}:{shaders.lutBuilderHdrPS.name}:{shaders.stopNanPS == null}:" +
                       $"{data.postProcessData.textures.filmGrainTex.Length}");
            Notes.Note($"post:{hasBloom}:{hasTonemapping}:{hasAdjustments}:{bloom.intensity.value}:{tonemapping.mode.value}:{tonemapping.mode.GetType().Name}:" +
                       $"{bloom.downscale.value}:{bloom.downscale.GetType().Name}:{bloom.parameters.Count}:{tonemapping.parameters.Count}:" +
                       $"{adjustments.parameters.Count}:{adjustments.saturation.value}");
        }
    }
}
