#include <Support/GpuFrame.h>
#include <Support/TestScene.h>

#include <Cpp/CppFeatures.h>

#include <Helix/Engine/Rendering/RenderSettings.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/RenderPipelines/Core/Volume/Volume.h>
#include <Helix/RenderPipelines/Standard/Components/AdditionalData.h>
#include <Helix/RenderPipelines/Standard/Overrides/Overrides.h>
#include <Helix/Testing/Test.h>

#include <cstdlib>

using namespace helix;
using namespace helix::engine;
using namespace helix::rendering;
using namespace helix::rendering::universal;
using namespace helix::rpstandard::testing;

namespace helix::rpstandard::testing
{
    Mesh* CppTestMeshes::Box() { return TestBox(); }
    Mesh* CppTestMeshes::Sphere() { return TestSphere(); }
}

namespace
{
    u32 MaxDifference(const Array<u32>& a, const Array<u32>& b)
    {
        u32 worst = 0;
        for (usize i = 0; i < a.Size() && i < b.Size(); i++)
            for (u32 c = 0; c < 24; c += 8)
                worst = Max(worst, u32(std::abs(i32((a[i] >> c) & 0xFFu) - i32((b[i] >> c) & 0xFFu))));
        return worst;
    }

    // The final test scene in C++ (FinalScene.cs builds the same through the C# API): Lit, Simple Lit and Unlit
    // objects, the sun's soft shadows, a point and a spot light, a skybox, a global volume with bloom, ACES and color
    // adjustments.
    struct FinalScene
    {
        Array<GameObject*> made;
        Array<Object*> assets;

        GameObject* Thing(StringView name, Mesh* mesh, Material* material, Vector3 position, Vector3 angles, Vector3 scale)
        {
            GameObject* g = GameObject::Create(name);
            g->transform()->SetPosition(position);
            g->transform()->SetRotation(Quaternion::Euler(angles.x, angles.y, angles.z));
            g->transform()->SetLocalScale(scale);
            g->AddComponent<MeshFilter>()->SetSharedMesh(mesh);
            g->AddComponent<MeshRenderer>()->SetSharedMaterial(material);
            made.Push(g);
            return g;
        }

        Material* Make(const char* shader, Color color)
        {
            Material* m = Material::Create(Shader::Find(shader));
            m->SetColor("_BaseColor", color);
            assets.Push(m);
            return m;
        }

        Light* MakeLight(StringView name, LightType type, Vector3 position, Vector3 angles, Color color, f32 intensity)
        {
            GameObject* g = GameObject::Create(name);
            g->transform()->SetPosition(position);
            g->transform()->SetRotation(Quaternion::Euler(angles.x, angles.y, angles.z));
            Light* l = g->AddComponent<Light>();
            l->SetType(type);
            l->SetColor(color);
            l->SetIntensity(intensity);
            made.Push(g);
            return l;
        }

        FinalScene()
        {
            GameObject* eye = GameObject::Create("FinalEye");
            made.Push(eye);
            eye->transform()->SetPosition(Vector3(0, 2.0f, -4.6f));
            eye->transform()->SetRotation(Quaternion::Euler(18, 0, 0));
            Camera* camera = eye->AddComponent<Camera>();
            camera->SetFieldOfView(60);
            camera->SetClearFlags(CameraClearFlags::Skybox);
            GetUniversalAdditionalCameraData(camera)->SetRenderPostProcessing(true);

            Light* sun = MakeLight("FinalSun", LightType::Directional, Vector3(0, 5, 0), Vector3(40, 35, 0), Color(1, 0.95f, 0.85f, 1), 1.2f);
            sun->SetShadows(LightShadows::Soft);
            Light* point = MakeLight("FinalPoint", LightType::Point, Vector3(2.2f, 0.9f, 0.8f), Vector3(0, 0, 0), Color(1, 0.45f, 0.2f, 1), 2.5f);
            point->SetRange(4);
            Light* spot = MakeLight("FinalSpot", LightType::Spot, Vector3(-2.4f, 3.2f, 1.5f), Vector3(65, 10, 0), Color(0.35f, 0.6f, 1, 1), 4);
            spot->SetRange(9);
            spot->SetSpotAngle(50);

            Mesh* box = TestBox();
            Mesh* sphere = TestSphere();
            assets.Push(box), assets.Push(sphere);
            Material* ground = Make("Universal Render Pipeline/Lit", Color(0.65f, 0.65f, 0.65f, 1));
            ground->SetFloat("_Smoothness", 0.25f);
            Thing("FinalGround", box, ground, Vector3(0, -0.1f, 2), Vector3(0, 0, 0), Vector3(14, 0.2f, 14));
            Material* metal = Make("Universal Render Pipeline/Lit", Color(0.95f, 0.85f, 0.6f, 1));
            metal->SetFloat("_Metallic", 1);
            metal->SetFloat("_Smoothness", 0.5f);
            Thing("FinalMetal", sphere, metal, Vector3(-1.6f, 0.6f, 1.2f), Vector3(0, 0, 0), Vector3(1.2f, 1.2f, 1.2f));
            Material* simple = Make("Universal Render Pipeline/Simple Lit", Color(0.25f, 0.75f, 0.35f, 1));
            simple->EnableKeyword("_SPECULAR_COLOR");
            simple->SetColor("_SpecColor", Color(1, 1, 1, 1));
            simple->SetFloat("_Smoothness", 0.6f);
            Thing("FinalSimple", box, simple, Vector3(0.1f, 0.5f, 2.6f), Vector3(0, 35, 0), Vector3(1, 1, 1));
            Thing("FinalGlow", box, Make("Universal Render Pipeline/Unlit", Color(3, 1.6f, 0.5f, 1)), Vector3(1.6f, 0.35f, 0.4f), Vector3(0, 0, 0),
                  Vector3(0.5f, 0.5f, 0.5f));
            Material* blue = Make("Universal Render Pipeline/Lit", Color(0.2f, 0.35f, 0.95f, 1));
            blue->SetFloat("_Smoothness", 0.5f);
            Thing("FinalBlue", sphere, blue, Vector3(-0.2f, 0.45f, 0.5f), Vector3(0, 0, 0), Vector3(0.9f, 0.9f, 0.9f))->SetLayer(4);

            // sRGB, as C#'s new Cubemap(1, RGBA32, false) makes it.
            Cubemap* cube = Cubemap::Create(1, TextureFormat::RGBA32, false, false);
            cube->SetFilterMode(FilterMode::Point);
            const Color sides[6] = {Color(0.5f, 0.6f, 0.8f, 1), Color(0.5f, 0.6f, 0.8f, 1), Color(0.3f, 0.5f, 0.9f, 1),
                                    Color(0.25f, 0.22f, 0.2f, 1), Color(0.6f, 0.7f, 0.85f, 1), Color(0.6f, 0.7f, 0.85f, 1)};
            for (u32 f = 0; f < 6; f++)
                cube->SetPixel(CubemapFace(f), 0, 0, sides[f]);
            cube->Apply(false);
            assets.Push(cube);
            Material* sky = Material::Create(Shader::Find("Skybox/Cubemap"));
            sky->SetTexture("_Tex", cube);
            assets.Push(sky);
            RenderSettings::SetSkybox(sky);
            RenderSettings::SetAmbientMode(AmbientMode::Flat);
            RenderSettings::SetAmbientLight(Color(0.15f, 0.16f, 0.2f, 1));
            RenderSettings::SetDefaultReflectionMode(DefaultReflectionMode::Skybox);

            auto* profile = static_cast<VolumeProfile*>(ScriptableObject::CreateInstance(scripting::TypeOf<VolumeProfile>()));
            assets.Push(profile);
            Bloom* bloom = profile->Add<Bloom>();
            bloom->intensity.Override(0.4f);
            bloom->threshold.Override(1.0f);
            bloom->scatter.Override(0.6f);
            profile->Add<Tonemapping>()->mode.Override(TonemappingMode::ACES);
            ColorAdjustments* adjustments = profile->Add<ColorAdjustments>();
            adjustments->postExposure.Override(0.4f);
            adjustments->contrast.Override(10.0f);
            adjustments->saturation.Override(10.0f);
            GameObject* volumeHolder = GameObject::Create("FinalVolume");
            made.Push(volumeHolder);
            auto* volume = static_cast<Volume*>(volumeHolder->AddComponent(scripting::TypeOf<Volume>()));
            volume->SetIsGlobal(true);
            volume->SetSharedProfile(profile);
        }

        ~FinalScene()
        {
            RenderSettings::SetSkybox(nullptr);
            for (GameObject* g : made)
                DestroyImmediate(g);
            for (Object* o : assets)
                DestroyImmediate(o);
        }
    };

    // The asset of the final scene: two soft shadow cascades, HDR, 4x MSAA.
    UniversalRenderPipelineAsset* FinalAsset(Array<Object*>& keep)
    {
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        keep.Push(asset->rendererDataList()[0]), keep.Push(asset);
        asset->SetShadowDistance(25);
        asset->SetShadowCascadeCount(2);
        asset->SetMainLightShadowmapResolution(2048);
        asset->SetSupportsSoftShadows(true);
        asset->SetSoftShadowQuality(SoftShadowQuality::Medium);
        asset->SetSupportsHDR(true);
        asset->SetMsaaSampleCount(4);
        return asset;
    }

    void DestroyAll(Array<Object*>& objects)
    {
        GraphicsSettings::SetDefaultRenderPipeline(nullptr);
        for (Object* o : objects)
            DestroyImmediate(o);
        objects.Clear();
    }
}

HX_TEST(Standard, TheFinalSceneInBothLanguagesOnEveryDevice)
{
    // The final scene built in C++ and by a C# script: the same image (golden StandardFinalScene) on every device.
    // A feature of each language on it: a C++ one drawing layer 4 in a flat color after the opaques, a C# one tinting
    // the image after post processing (StandardFinalSceneFeatures). C# changing the pipeline's settings at run time:
    // render scale, shadow distance and cascades, MSAA, HDR and the volume's bloom (StandardFinalSceneChanged).
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 128, height = 80;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        Array<Object*> keep;
        UniversalRenderPipelineAsset* asset = FinalAsset(keep);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        Array<u32> fromCpp;
        {
            FinalScene scene;
            HX_REQUIRE(target.Render());
            HX_CHECK(target.Matches("StandardFinalScene", label));
            fromCpp = target.pixels;
        }

        // The same scene from C#.
        GameObject* host = GameObject::Create("FinalHost");
        host->AddComponent(CSharp("CsFinalScene").type);
        RunScriptPhase(ScriptMessage::Update);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardFinalScene", label));
        u32 between = MaxDifference(fromCpp, target.pixels);
        HX_CHECK(between <= 2);
        if (between > 2)
            std::printf("    the C# scene differs from the C++ one by %u\n", between);

        // A feature of each language.
        auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
        auto* cpp = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(scripting::TypeOf<CppOverrideFeature>()));
        auto* cs = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(CSharp("CsTintFeature").type));
        HX_REQUIRE(cpp && cs);
        keep.Push(cpp), keep.Push(cs);
        Array<ScriptableRendererFeature*> features;
        features.Push(cpp), features.Push(cs);
        data->SetRendererFeatures(features);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardFinalSceneFeatures", label));
        data->SetRendererFeatures(Array<ScriptableRendererFeature*>());

        // C# changing the settings at run time.
        Notes().Clear();
        GameObject* changer = GameObject::Create("FinalChanger");
        changer->AddComponent(CSharp("CsFinalSettings").type);
        RunScriptPhase(ScriptMessage::Update);
        DestroyImmediate(changer);
        HX_CHECK(SameNotes({"settings:0.75:12:1:1:False:1.5"}));
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardFinalSceneChanged", label));

        // The light helpers from C#: the C++ ones' values for the spot light.
        {
            Light* spot = GameObject::Find("FinalSpot")->GetComponent<Light>();
            Vector4 attenuation, direction;
            UniversalRenderPipeline::GetLightAttenuationAndSpotDirection(LightType::Spot, spot->range(), spot->transform()->localToWorldMatrix(),
                                                                         spot->spotAngle(), nullptr, attenuation, direction);
            String expected = Format("lights:True:{}:{}:{}:{}:{}:{}:{}", Milli(attenuation.x), Milli(attenuation.y), Milli(attenuation.z), Milli(attenuation.w),
                                     Milli(direction.x), Milli(direction.y), Milli(direction.z));
            Notes().Clear();
            GameObject* helpers = GameObject::Create("FinalHelpers");
            helpers->AddComponent(CSharp("CsLightHelpers").type);
            RunScriptPhase(ScriptMessage::Update);
            DestroyImmediate(helpers);
            bool same = Notes().Size() == 1 && Notes()[0] == expected.View();
            if (!same)
                for (const String& n : Notes())
                    std::printf("    %s (expected %s)\n", n.CStr(), expected.CStr());
            HX_CHECK(same);
            Notes().Clear();
        }

        DestroyImmediate(host);
        RunScriptPhase(ScriptMessage::Update);
        DestroyAll(keep);
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, GpuBudgetAt1080p)
{
    // The final scene at 1920 x 1080 (soft shadows, the lights, the skybox, bloom and ACES): the GPU time of its frames
    // and their passes on every device; with HELIX_GPU_BUDGET=1 (the hardware run of the exit check) a frame takes at
    // most 16.6 ms (60 fps).
    HX_REQUIRE(StartScripts());
    bool checked = engine::testing::GpuBudgetChecked();
    u32 ran = 0;
    ForEachDevice([&](rhi::Device& device, StringView label) {
        if (!device.Caps().gpuTimers)
            return;
        ran++;
        ShaderFolder shaders;
        TestTarget target(device, 1920, 1080);
        Array<Object*> keep;
        GraphicsSettings::SetDefaultRenderPipeline(FinalAsset(keep));
        {
            FinalScene scene;
            auto draw = [] {
                UpdateWorldMatrices();
                Graphics::RenderFrame();
            };
            // The first frames make the shaders' variants and the pools.
            engine::testing::TimeGpuFrames(device, 4, draw);
            engine::testing::GpuFrameTimes times = engine::testing::TimeGpuFrames(device, 16, draw);
            HX_REQUIRE(!times.frames.Empty());
            engine::testing::PrintGpuFrames(label, times);
            HX_CHECK(!checked || times.Median() <= 16.6);
        }
        DestroyAll(keep);
    });
    std::printf("    %u devices with GPU timers%s\n", ran, checked ? ", budget checked" : "");
}
