using System.Collections.Generic;
using HelixEngine;
using HelixEngine.Rendering;
using HelixEngine.Rendering.RenderGraphModule;
using HelixEngine.Rendering.Universal;

namespace Tests
{
    // The final test scene through the C# API (FinalTest.cpp builds the same in C++).
    public class CsFinalScene : GameBehaviour
    {
        readonly List<GameObject> m_Made = new();
        readonly List<Object> m_Assets = new();

        GameObject Thing(string name, Mesh mesh, Material material, Vector3 position, Vector3 angles, Vector3 scale)
        {
            var g = new GameObject(name);
            g.transform.position = position;
            g.transform.rotation = Quaternion.Euler(angles.x, angles.y, angles.z);
            g.transform.localScale = scale;
            g.AddComponent<MeshFilter>().sharedMesh = mesh;
            g.AddComponent<MeshRenderer>().sharedMaterial = material;
            m_Made.Add(g);
            return g;
        }

        Material Make(string shader, Color color)
        {
            var m = new Material(Shader.Find(shader));
            m.SetColor("_BaseColor", color);
            m_Assets.Add(m);
            return m;
        }

        Light MakeLight(string name, LightType type, Vector3 position, Vector3 angles, Color color, float intensity)
        {
            var g = new GameObject(name);
            g.transform.position = position;
            g.transform.rotation = Quaternion.Euler(angles.x, angles.y, angles.z);
            Light l = g.AddComponent<Light>();
            l.type = type;
            l.color = color;
            l.intensity = intensity;
            m_Made.Add(g);
            return l;
        }

        void Start()
        {
            var eye = new GameObject("FinalEye");
            m_Made.Add(eye);
            eye.transform.position = new Vector3(0, 2.0f, -4.6f);
            eye.transform.rotation = Quaternion.Euler(18, 0, 0);
            Camera camera = eye.AddComponent<Camera>();
            camera.fieldOfView = 60;
            camera.clearFlags = CameraClearFlags.Skybox;
            camera.GetUniversalAdditionalCameraData().renderPostProcessing = true;

            Light sun = MakeLight("FinalSun", LightType.Directional, new Vector3(0, 5, 0), new Vector3(40, 35, 0), new Color(1, 0.95f, 0.85f, 1), 1.2f);
            sun.shadows = LightShadows.Soft;
            Light point = MakeLight("FinalPoint", LightType.Point, new Vector3(2.2f, 0.9f, 0.8f), new Vector3(0, 0, 0), new Color(1, 0.45f, 0.2f, 1), 2.5f);
            point.range = 4;
            Light spot = MakeLight("FinalSpot", LightType.Spot, new Vector3(-2.4f, 3.2f, 1.5f), new Vector3(65, 10, 0), new Color(0.35f, 0.6f, 1, 1), 4);
            spot.range = 9;
            spot.spotAngle = 50;

            Mesh box = StandardTests.CppTestMeshes.Box();
            Mesh sphere = StandardTests.CppTestMeshes.Sphere();
            m_Assets.Add(box);
            m_Assets.Add(sphere);
            Material ground = Make("Universal Render Pipeline/Lit", new Color(0.65f, 0.65f, 0.65f, 1));
            ground.SetFloat("_Smoothness", 0.25f);
            Thing("FinalGround", box, ground, new Vector3(0, -0.1f, 2), new Vector3(0, 0, 0), new Vector3(14, 0.2f, 14));
            Material metal = Make("Universal Render Pipeline/Lit", new Color(0.95f, 0.85f, 0.6f, 1));
            metal.SetFloat("_Metallic", 1);
            metal.SetFloat("_Smoothness", 0.5f);
            Thing("FinalMetal", sphere, metal, new Vector3(-1.6f, 0.6f, 1.2f), new Vector3(0, 0, 0), new Vector3(1.2f, 1.2f, 1.2f));
            Material simple = Make("Universal Render Pipeline/Simple Lit", new Color(0.25f, 0.75f, 0.35f, 1));
            simple.EnableKeyword("_SPECULAR_COLOR");
            simple.SetColor("_SpecColor", new Color(1, 1, 1, 1));
            simple.SetFloat("_Smoothness", 0.6f);
            Thing("FinalSimple", box, simple, new Vector3(0.1f, 0.5f, 2.6f), new Vector3(0, 35, 0), new Vector3(1, 1, 1));
            Thing("FinalGlow", box, Make("Universal Render Pipeline/Unlit", new Color(3, 1.6f, 0.5f, 1)), new Vector3(1.6f, 0.35f, 0.4f), new Vector3(0, 0, 0),
                  new Vector3(0.5f, 0.5f, 0.5f));
            Material blue = Make("Universal Render Pipeline/Lit", new Color(0.2f, 0.35f, 0.95f, 1));
            blue.SetFloat("_Smoothness", 0.5f);
            Thing("FinalBlue", sphere, blue, new Vector3(-0.2f, 0.45f, 0.5f), new Vector3(0, 0, 0), new Vector3(0.9f, 0.9f, 0.9f)).layer = 4;

            var cube = new Cubemap(1, TextureFormat.RGBA32, false) { filterMode = FilterMode.Point };
            Color[] sides =
            {
                new Color(0.5f, 0.6f, 0.8f, 1), new Color(0.5f, 0.6f, 0.8f, 1), new Color(0.3f, 0.5f, 0.9f, 1), new Color(0.25f, 0.22f, 0.2f, 1),
                new Color(0.6f, 0.7f, 0.85f, 1), new Color(0.6f, 0.7f, 0.85f, 1),
            };
            for (int f = 0; f < 6; f++)
                cube.SetPixel((CubemapFace)f, 0, 0, sides[f]);
            cube.Apply(false);
            m_Assets.Add(cube);
            var sky = new Material(Shader.Find("Skybox/Cubemap"));
            sky.SetTexture("_Tex", cube);
            m_Assets.Add(sky);
            RenderSettings.skybox = sky;
            RenderSettings.ambientMode = AmbientMode.Flat;
            RenderSettings.ambientLight = new Color(0.15f, 0.16f, 0.2f, 1);
            RenderSettings.defaultReflectionMode = DefaultReflectionMode.Skybox;

            var profile = ScriptableObject.CreateInstance<VolumeProfile>();
            m_Assets.Add(profile);
            Bloom bloom = profile.Add<Bloom>();
            bloom.intensity.Override(0.4f);
            bloom.threshold.Override(1.0f);
            bloom.scatter.Override(0.6f);
            profile.Add<Tonemapping>().mode.Override(TonemappingMode.ACES);
            ColorAdjustments adjustments = profile.Add<ColorAdjustments>();
            adjustments.postExposure.Override(0.4f);
            adjustments.contrast.Override(10.0f);
            adjustments.saturation.Override(10.0f);
            var volumeHolder = new GameObject("FinalVolume");
            m_Made.Add(volumeHolder);
            Volume volume = volumeHolder.AddComponent<Volume>();
            volume.isGlobal = true;
            volume.sharedProfile = profile;
        }

        void OnDestroy()
        {
            RenderSettings.skybox = null;
            foreach (GameObject g in m_Made)
                DestroyImmediate(g);
            foreach (Object o in m_Assets)
                DestroyImmediate(o);
        }
    }

    // Tints the image after post processing (Tests/StandardTint over the camera's color).
    public class CsTintPass : ScriptableRenderPass
    {
        static readonly int s_Tint = Shader.PropertyToID("_TestTint");
        Material m_Material;

        class PassData
        {
            public TextureHandle source;
            public Material material;
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
        {
            if (m_Material == null)
                m_Material = new Material(Shader.Find("Tests/StandardTint"));
            UniversalResourceData resources = frameData.Get<UniversalResourceData>();
            TextureHandle source = resources.activeColorTexture;
            TextureHandle tinted = renderGraph.CreateTexture(renderGraph.GetTextureDesc(source));
            using (IRasterRenderGraphBuilder builder = renderGraph.AddRasterRenderPass("CsTint", out PassData data))
            {
                data.source = source;
                data.material = m_Material;
                builder.UseTexture(source);
                builder.SetRenderAttachment(tinted, 0);
                builder.AllowGlobalStateModification(true);
                builder.SetRenderFunc((PassData d, RasterGraphContext ctx) =>
                {
                    ctx.cmd.SetGlobalVector(s_Tint, new Vector4(1.0f, 0.8f, 0.9f, 1.0f));
                    Blitter.BlitTexture(ctx.cmd, d.source, new Vector4(1, 1, 0, 0), d.material, 0);
                });
            }
            resources.cameraColor = tinted;
        }

        public void Release()
        {
            if (m_Material != null)
                Object.DestroyImmediate(m_Material);
        }
    }

    public class CsTintFeature : ScriptableRendererFeature
    {
        CsTintPass m_Pass;

        public override void Create() => m_Pass = new CsTintPass { renderPassEvent = RenderPassEvent.AfterRenderingPostProcessing };

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData) => renderer.EnqueuePass(m_Pass);

        protected override void Dispose(bool disposing) => m_Pass?.Release();
    }

    // The pipeline's settings changed from C# at run time, read back.
    public class CsFinalSettings : GameBehaviour
    {
        void Start()
        {
            var asset = (UniversalRenderPipelineAsset)GraphicsSettings.defaultRenderPipeline;
            asset.renderScale = 0.75f;
            asset.shadowDistance = 12;
            asset.shadowCascadeCount = 1;
            asset.msaaSampleCount = 1;
            asset.supportsHDR = false;
            VolumeProfile profile = GameObject.Find("FinalVolume").GetComponent<Volume>().sharedProfile;
            profile.TryGet(out Bloom bloom);
            bloom.intensity.Override(1.5f);
            Notes.Note($"settings:{asset.renderScale}:{asset.shadowDistance}:{asset.shadowCascadeCount}:{asset.msaaSampleCount}:{asset.supportsHDR}:" +
                       $"{bloom.intensity.value}");
        }
    }
}

namespace Tests
{
    // The pipeline's light helpers from C#: the final scene's spot light (thousandths).
    public class CsLightHelpers : GameBehaviour
    {
        static long M(float v) => (long)System.Math.Round(v * 1000.0, System.MidpointRounding.AwayFromZero);

        void Start()
        {
            Light spot = GameObject.Find("FinalSpot").GetComponent<Light>();
            UniversalRenderPipeline.GetLightAttenuationAndSpotDirection(LightType.Spot, spot.range, spot.transform.localToWorldMatrix, spot.spotAngle, null,
                                                                        out Vector4 attenuation, out Vector4 direction);
            Notes.Note($"lights:{UniversalRenderPipeline.IsGameCamera(GameObject.Find("FinalEye").GetComponent<Camera>())}:{M(attenuation.x)}:{M(attenuation.y)}:" +
                       $"{M(attenuation.z)}:{M(attenuation.w)}:{M(direction.x)}:{M(direction.y)}:{M(direction.z)}");
        }
    }
}
