using System;
using HelixEngine;
using HelixEngine.Rendering;
using HelixEngine.Rendering.RenderGraphModule;
using HelixEngine.Rendering.Universal;

namespace Tests
{
    // Notes the main light's cascades as the C# ShadowUtils makes them (the same text as the C++ pass's notes).
    public class CsShadowPass : ScriptableRenderPass
    {
        static long Milli(float v, double scale = 1000.0) => (long)Math.Round(v * scale, MidpointRounding.AwayFromZero);

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
        {
            UniversalRenderingData rendering = frameData.Get<UniversalRenderingData>();
            UniversalLightData lights = frameData.Get<UniversalLightData>();
            UniversalShadowData shadows = frameData.Get<UniversalShadowData>();
            if (lights.mainLightIndex < 0 || !shadows.supportsMainLightShadows)
                return;
            int count = shadows.mainLightShadowCascadesCount;
            int width = shadows.mainLightShadowmapWidth, height = shadows.mainLightShadowmapHeight;
            int tile = ShadowUtils.GetMaxTileResolutionInAtlas(width, height, count);
            Notes.Note($"atlas:{count}:{width}:{height}:{tile}");
            CullingResults culling = rendering.cullResults;
            VisibleLight light = lights.visibleLights[lights.mainLightIndex];
            for (int i = 0; i < count; i++)
            {
                bool ok = ShadowUtils.ExtractDirectionalLightMatrix(ref culling, shadows, lights.mainLightIndex, i, width, height, tile, light.light.shadowNearPlane,
                                                                    out Vector4 sphere, out ShadowSliceData slice);
                Vector4 bias = ShadowUtils.GetShadowBias(ref light, lights.mainLightIndex, shadows, slice.projectionMatrix, tile);
                Notes.Note($"cascade:{i}:{(ok ? 1 : 0)}:{slice.offsetX}:{slice.offsetY}:{slice.resolution}:{Milli(sphere.x)}:{Milli(sphere.y)}:{Milli(sphere.z)}:" +
                           $"{Milli(sphere.w)}:{Milli(slice.shadowTransform.m00, 10000)}:{Milli(slice.shadowTransform.m03, 10000)}:" +
                           $"{Milli(slice.shadowTransform.m13, 10000)}:{Milli(bias.x, 100000)}:{Milli(bias.y, 100000)}");
            }
        }
    }

    public class CsShadowFeature : ScriptableRendererFeature
    {
        CsShadowPass m_Pass;

        public override void Create() => m_Pass = new CsShadowPass { renderPassEvent = RenderPassEvent.BeforeRenderingShadows };

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData) => renderer.EnqueuePass(m_Pass);
    }

    // ShadowUtils without a frame: tile sizes of atlases, a tile's transform, a cleared slice.
    public class CsShadowUtilsValues : GameBehaviour
    {
        void Start()
        {
            Notes.Note($"tiles:{ShadowUtils.GetMaxTileResolutionInAtlas(1024, 1024, 1)}:{ShadowUtils.GetMaxTileResolutionInAtlas(1024, 512, 2)}:" +
                       $"{ShadowUtils.GetMaxTileResolutionInAtlas(1024, 1024, 3)}:{ShadowUtils.GetMaxTileResolutionInAtlas(1024, 1024, 4)}:" +
                       $"{ShadowUtils.GetMaxTileResolutionInAtlas(1024, 1024, 5)}:{ShadowUtils.GetMaxTileResolutionInAtlas(0, 1024, 1)}");
            ShadowSliceData slice = default;
            slice.Clear();
            slice.offsetX = 512;
            slice.offsetY = 256;
            slice.resolution = 256;
            ShadowUtils.ApplySliceTransform(ref slice, 1024, 512);
            Vector4 p = slice.shadowTransform * new Vector4(0.5f, 0.5f, 0.25f, 1);
            Notes.Note($"slice:{p.x}:{p.y}:{p.z}:{p.w}");
        }
    }
}
