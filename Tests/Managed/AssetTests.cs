using HelixEngine;
using HelixEngine.Rendering;
using HelixEngine.Rendering.RenderGraphModule;
using HelixEngine.Rendering.Universal;

namespace Tests
{
    // A pass of C#, recorded on the native renderer's graph: it notes what it sees of the frame.
    public class CsNotePass : ScriptableRenderPass
    {
        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
        {
            UniversalCameraData camera = frameData.Get<UniversalCameraData>();
            UniversalResourceData resources = frameData.Get<UniversalResourceData>();
            Notes.Note($"cs:{(int)renderPassEvent}:{camera.scaledWidth}:{(resources.cameraColor.IsValid() ? "color" : "none")}:{renderGraph != null}");
        }
    }

    // A feature of C#: a C# class below the C++ feature class, queueing its C# pass before the transparent objects.
    public class CsTestFeature : ScriptableRendererFeature
    {
        CsNotePass m_Pass;

        public override void Create() => m_Pass = new CsNotePass { renderPassEvent = RenderPassEvent.BeforeRenderingTransparents };

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            if (renderingData.cameraData.cameraType == CameraType.Game)
                renderer.EnqueuePass(m_Pass);
        }
    }

    // The pipeline's settings from C#: the asset as a RenderPipelineAsset, the renderer data, the features, the
    // pipeline's constants, the camera and light extensions.
    public class CsAssetTests : GameBehaviour
    {
        void Start()
        {
            UniversalRenderPipelineAsset asset = UniversalRenderPipelineAsset.Create();
            RenderPipelineAsset general = asset;
            asset.renderScale = 5;
            Notes.Note($"asset:{general != null}:{asset.defaultShader == null}:{general.renderPipelineShaderTag}:{general.pipelineType.Name}:" +
                       $"{UniversalRenderPipelineAsset.k_MinLutSize}:{UniversalRenderPipelineAsset.k_MaxLutSize}:{asset.rendererDataList.Length}:{asset.renderScale}");

            var data = (UniversalRendererData)asset.rendererDataList[0];
            data.transparentLayerMask = 5;
            Notes.Note($"data:{asset.rendererDataList.Length}:{data.renderingMode}:{data.opaqueLayerMask.value}:{data.transparentLayerMask.value}");

            int before = data.rendererFeatures.Count;
            var feature = ScriptableObject.CreateInstance<CsTestFeature>();
            data.rendererFeatures.Add(feature);
            data.SetDirty();
            bool found = data.TryGetRendererFeature(out CsTestFeature f) && f == feature;
            feature.SetActive(false);
            Notes.Note($"features:{before}:{data.rendererFeatures.Count}:{found}:{feature.isActive}");

            GraphicsSettings.defaultRenderPipeline = asset;
            Notes.Note($"pipeline:{UniversalRenderPipeline.asset == asset}:{UniversalRenderPipeline.maxPerObjectLights}:{UniversalRenderPipeline.minRenderScale}:" +
                       $"{UniversalRenderPipeline.maxRenderScale}:{UniversalRenderPipeline.maxVisibleAdditionalLights}");

            var eye = new GameObject("CsEye");
            Camera camera = eye.AddComponent<Camera>();
            UniversalAdditionalCameraData cameraData = camera.GetUniversalAdditionalCameraData();
            Notes.Note($"camera:{cameraData == camera.GetUniversalAdditionalCameraData()}:{cameraData.renderShadows}:{cameraData.requiresDepthTexture}:" +
                       $"{cameraData.renderType}:{cameraData.volumeLayerMask.value}:{cameraData.requiresVolumeFrameworkUpdate}");

            var lamp = new GameObject("CsLamp");
            Light light = lamp.AddComponent<Light>();
            UniversalAdditionalLightData lightData = light.GetUniversalAdditionalLightData();
            Notes.Note($"light:{lightData == light.GetUniversalAdditionalLightData()}:{lightData.usePipelineSettings}:{lightData.softShadowQuality}");

            GraphicsSettings.defaultRenderPipeline = null;
            Object.DestroyImmediate(eye);
            Object.DestroyImmediate(lamp);
            Object.DestroyImmediate(feature);
            Object.DestroyImmediate(data);
            Object.DestroyImmediate(asset);
        }
    }
}
