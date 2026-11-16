#include <Support/TestScene.h>

#include <Helix/Engine/Rendering/ReflectionProbe.h>
#include <Helix/Engine/Rendering/RenderSettings.h>
#include <Helix/Testing/Test.h>

#include <cmath>

using namespace helix;
using namespace helix::engine;
using namespace helix::rendering::universal;
using namespace helix::rpstandard::testing;

namespace
{
    // The material values of a surface of the test grid.
    struct Look
    {
        const char* shader;
        Color color;
        std::initializer_list<const char*> keywords;
        void (*set)(Material& m, Array<Object*>& made);
    };

    void Transparent(Material& m, BlendMode source, BlendMode destination)
    {
        m.SetFloat("_Surface", 1);
        m.SetFloat("_SrcBlend", f32(source));
        m.SetFloat("_DstBlend", f32(destination));
        m.SetFloat("_SrcBlendAlpha", f32(source == BlendMode::DstColor ? BlendMode::Zero : BlendMode::One));
        m.SetFloat("_DstBlendAlpha", f32(destination));
        m.SetFloat("_ZWrite", 0);
        m.SetRenderQueue(3000);
        m.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
    }
}

HX_TEST(Standard, TheShadersOfThePipelineOnEveryDevice)
{
    // Sixteen spheres over a lit ground under the sun (its shadow), a red point light and a green spot light, the flat
    // ambient light, a reflection probe around the second sphere and the scene's custom reflection elsewhere: Lit's
    // workflows, maps, smoothness sources, alpha clipping, transparent blending (alpha, premultiplied), render face and
    // switches; Simple Lit's Lambert and Blinn-Phong; Unlit, multiplied; Particles/Unlit (the default particle
    // material); a box with the asset's default material.
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 128, height = 112;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        TestScene scene;
        for (GameObject* g : scene.made)
            if (g->Name() != StringView("Eye") && g->Name() != StringView("Sun") && g->Name() != StringView("Lamp"))
                g->SetActive(false);
        scene.camera->transform()->SetPosition(Vector3(0, 6.5f, -6));
        scene.camera->transform()->SetRotation(Quaternion::Euler(45, 0, 0));
        GameObject* lamp = GameObject::Find("Lamp");
        lamp->GetComponent<Light>()->SetColor(Color(1, 0.2f, 0.2f, 1));
        lamp->transform()->SetPosition(Vector3(3.2f, 0.6f, 4));
        GameObject* spotHolder = GameObject::Create("Spot");
        scene.made.Push(spotHolder);
        Light* spot = spotHolder->AddComponent<Light>();
        spot->SetType(LightType::Spot);
        spot->SetColor(Color(0.2f, 1, 0.2f, 1));
        spot->SetIntensity(4);
        spot->SetRange(6);
        spot->SetSpotAngle(50);
        spotHolder->transform()->SetPosition(Vector3(-3, 3, 4.5f));
        spotHolder->transform()->SetRotation(Quaternion::Euler(90, 0, 0));
        RenderSettings::SetAmbientMode(AmbientMode::Flat);
        RenderSettings::SetAmbientLight(Color(0.12f, 0.12f, 0.16f, 1));
        Cubemap* sky = TestCube(Color(0.3f, 0.45f, 0.7f, 1));
        Cubemap* orange = TestCube(Color(1, 0.55f, 0.1f, 1));
        scene.assets.Push(sky), scene.assets.Push(orange);
        RenderSettings::SetDefaultReflectionMode(DefaultReflectionMode::Custom);
        RenderSettings::SetCustomReflectionTexture(sky);
        GameObject* probeHolder = GameObject::Create("Probe");
        scene.made.Push(probeHolder);
        probeHolder->transform()->SetPosition(Vector3(-1, 0.5f, 0));
        ReflectionProbe* probe = probeHolder->AddComponent<ReflectionProbe>();
        probe->SetSize(Vector3(1.4f, 1.4f, 1.4f));
        probe->SetMode(ReflectionProbeMode::Custom);
        probe->SetCustomBakedTexture(orange);

        // The maps: a checker (alpha clipping), bumps (normal map), stripes of metal and gloss, an occlusion and an
        // emission gradient, a base map whose alpha is the smoothness.
        // The checker point sampled (bilinear alpha near the cutoff differs between devices), its edges off the test
        // sphere's mesh lines (u steps of 1/24, v of 1/12): on a triangle edge a coordinate exactly at a check's edge
        // rounds to either side depending on the device. Shifted by 0.3 of a check, the edges fall between them.
        Texture2D* checker = TestTexture(256, true, [](f32 x, f32 y) {
            bool on = (i32(x * 4 + 0.3f) + i32(y * 4 + 0.3f)) % 2 == 0;
            return Color(1, 1, 1, on ? 1.0f : 0.0f);
        }, FilterMode::Point);
        Texture2D* bumps = TestTexture(32, true, [](f32 x, f32) {
            f32 slope = std::sin(x * 6.2831853f * 4.0f) * 0.7f;
            Vector3 nm = Vector3(slope, 0, 1).normalized();
            return Color(nm.x * 0.5f + 0.5f, nm.y * 0.5f + 0.5f, nm.z * 0.5f + 0.5f, 1);
        });
        Texture2D* metalStripes = TestTexture(8, true, [](f32 x, f32) { return x < 0.5f ? Color(1, 0, 0, 0.9f) : Color(0, 0, 0, 0.2f); });
        Texture2D* occlusion = TestTexture(8, true, [](f32, f32 y) { return Color(0, y, 0, 1); });
        Texture2D* emission = TestTexture(8, false, [](f32 x, f32) { return Color(0.1f, 0.4f * x, 1, 1); });
        Texture2D* glossyBase = TestTexture(8, false, [](f32 x, f32) { return Color(0.9f, 0.9f, 0.9f, x); });
        for (Object* t : {static_cast<Object*>(checker), static_cast<Object*>(bumps), static_cast<Object*>(metalStripes), static_cast<Object*>(occlusion),
                          static_cast<Object*>(emission), static_cast<Object*>(glossyBase)})
            scene.assets.Push(t);
        static Texture2D *sChecker, *sBumps, *sMetal, *sOcclusion, *sEmission, *sGlossy;
        sChecker = checker, sBumps = bumps, sMetal = metalStripes, sOcclusion = occlusion, sEmission = emission, sGlossy = glossyBase;

        const char* lit = "Universal Render Pipeline/Lit";
        const char* simple = "Universal Render Pipeline/Simple Lit";
        const char* unlit = "Universal Render Pipeline/Unlit";
        const Look looks[] = {
            {lit, Color(0.9f, 0.9f, 0.9f, 1), {}, nullptr},
            {lit, Color(0.9f, 0.9f, 0.9f, 1), {}, [](Material& m, Array<Object*>&) { m.SetFloat("_Metallic", 1), m.SetFloat("_Smoothness", 0.95f); }},
            {lit, Color(0.2f, 0.2f, 0.2f, 1), {"_SPECULAR_SETUP"},
             [](Material& m, Array<Object*>&) { m.SetColor("_SpecColor", Color(0.9f, 0.2f, 0.2f, 1)), m.SetFloat("_Smoothness", 0.8f); }},
            {lit, Color(0.8f, 0.8f, 0.8f, 1), {"_NORMALMAP"}, [](Material& m, Array<Object*>&) { m.SetTexture("_BumpMap", sBumps); }},
            {lit, Color(0.9f, 0.8f, 0.6f, 1), {"_METALLICSPECGLOSSMAP"}, [](Material& m, Array<Object*>&) { m.SetTexture("_MetallicGlossMap", sMetal); }},
            {lit, Color(0.8f, 0.8f, 0.8f, 1), {"_OCCLUSIONMAP", "_EMISSION"},
             [](Material& m, Array<Object*>&) {
                 m.SetTexture("_OcclusionMap", sOcclusion), m.SetTexture("_EmissionMap", sEmission), m.SetColor("_EmissionColor", Color(0.6f, 0.6f, 0.6f, 1));
             }},
            {lit, Color(0.9f, 0.5f, 0.9f, 1), {"_ALPHATEST_ON"}, [](Material& m, Array<Object*>&) { m.SetTexture("_BaseMap", sChecker); }},
            {lit, Color(0.2f, 0.6f, 1, 0.5f), {}, [](Material& m, Array<Object*>&) { Transparent(m, BlendMode::SrcAlpha, BlendMode::OneMinusSrcAlpha); }},
            {lit, Color(1, 0.8f, 0.2f, 0.3f), {"_ALPHAPREMULTIPLY_ON"},
             [](Material& m, Array<Object*>&) { Transparent(m, BlendMode::One, BlendMode::OneMinusSrcAlpha); }},
            {lit, Color(0.7f, 0.9f, 0.7f, 1), {"_RECEIVE_SHADOWS_OFF", "_SPECULARHIGHLIGHTS_OFF", "_ENVIRONMENTREFLECTIONS_OFF"},
             [](Material& m, Array<Object*>&) { m.SetFloat("_Cull", f32(CullMode::Front)); }},
            {lit, Color(1, 1, 1, 1), {"_SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A"},
             [](Material& m, Array<Object*>&) { m.SetTexture("_BaseMap", sGlossy), m.SetFloat("_Metallic", 1), m.SetFloat("_Smoothness", 1); }},
            {simple, Color(0.3f, 0.9f, 0.3f, 1), {}, nullptr},
            {simple, Color(0.3f, 0.3f, 0.9f, 1), {"_SPECULAR_COLOR"},
             [](Material& m, Array<Object*>&) { m.SetColor("_SpecColor", Color(1, 1, 1, 1)), m.SetFloat("_Smoothness", 0.8f); }},
            {unlit, Color(1, 0.5f, 0, 1), {}, nullptr},
            {unlit, Color(0.5f, 0.5f, 1, 1), {"_ALPHAMODULATE_ON"}, [](Material& m, Array<Object*>&) { Transparent(m, BlendMode::DstColor, BlendMode::Zero); }},
            {nullptr, Color(1, 1, 1, 1), {}, nullptr},
        };
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        scene.assets.Push(asset->rendererDataList()[0]), scene.assets.Push(asset);
        asset->SetShadowDistance(20);
        asset->SetMainLightShadowmapResolution(1024);
        asset->SetSupportsHDR(false);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        HX_REQUIRE(asset->defaultMaterial() != nullptr && asset->defaultParticleMaterial() != nullptr && asset->defaultShader() == Shader::Find(lit));

        Mesh* sphere = TestSphere();
        scene.assets.Push(sphere);
        GameObject* ground = scene.Thing("Floor", scene.Colored(lit, Color(0.8f, 0.8f, 0.8f, 1)), Vector3(0, -0.05f, 2.5f), Quaternion::identity,
                                          Vector3(10, 0.1f, 8));
        (void)ground;
        u32 k = 0;
        for (const Look& look : looks)
        {
            Material* m = look.shader ? Material::Create(Shader::Find(look.shader)) : asset->defaultParticleMaterial();
            HX_REQUIRE(m != nullptr);
            if (look.shader)
                scene.assets.Push(m);
            m->SetColor("_BaseColor", look.color);
            for (const char* kw : look.keywords)
                m->EnableKeyword(kw);
            if (look.set)
                look.set(*m, scene.assets);
            Vector3 at(-3.0f + 2.0f * f32(k % 4), 0.5f, 1.6f * f32(k / 4));
            GameObject* g = GameObject::Create("Sphere");
            g->transform()->SetPosition(at);
            g->AddComponent<MeshFilter>()->SetSharedMesh(sphere);
            g->AddComponent<MeshRenderer>()->SetSharedMaterial(m);
            scene.made.Push(g);
            k++;
        }
        scene.Thing("Default", asset->defaultMaterial(), Vector3(4.6f, 0.4f, 2), Quaternion::Euler(0, 20, 0), Vector3(0.8f, 0.8f, 0.8f));

        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardShaders", label));
        RenderSettings::Apply(RenderSettingsData());
    });
    std::printf("    %u devices\n", devices);
}
