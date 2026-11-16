#include <Support/TestScene.h>

#include <Cpp/CppFeatures.h>

#include <Helix/Engine/Rendering/Light.h>
#include <Helix/Engine/Rendering/RenderSettings.h>
#include <Helix/RenderPipelines/Standard/Components/AdditionalData.h>
#include <Helix/RenderPipelines/Standard/Shadows/ShadowUtils.h>
#include <Helix/Testing/Test.h>

#include <cmath>
#include <cstring>

using namespace helix;
using namespace helix::engine;
using namespace helix::rendering::universal;
using namespace helix::rpstandard::testing;

namespace
{
    // The test scene with only its camera and sun, the sun at a rotation with its shadows, an asset of the pipeline.
    struct ShadowScene
    {
        TestScene scene;
        Light* sun = nullptr;
        UniversalRenderPipelineAsset* asset = nullptr;

        ShadowScene(Vector3 eye, Vector3 eyeAngles, Vector3 sunAngles, LightShadows shadows, f32 distance, i32 resolution)
        {
            for (GameObject* g : scene.made)
                if (g->Name() != StringView("Eye") && g->Name() != StringView("Sun"))
                    g->SetActive(false);
            scene.camera->transform()->SetPosition(eye);
            scene.camera->transform()->SetRotation(Quaternion::Euler(eyeAngles.x, eyeAngles.y, eyeAngles.z));
            GameObject* holder = GameObject::Find("Sun");
            holder->transform()->SetRotation(Quaternion::Euler(sunAngles.x, sunAngles.y, sunAngles.z));
            sun = holder->GetComponent<Light>();
            sun->SetShadows(shadows);
            asset = UniversalRenderPipelineAsset::Create();
            scene.assets.Push(asset->rendererDataList()[0]), scene.assets.Push(asset);
            asset->SetShadowDistance(distance);
            asset->SetMainLightShadowmapResolution(resolution);
            asset->SetShadowCascadeCount(1);
            asset->SetSupportsHDR(false);
            GraphicsSettings::SetDefaultRenderPipeline(asset);
            // The ambient light flat, no reflection (other tests change both).
            RenderSettings::SetAmbientMode(AmbientMode::Flat);
            RenderSettings::SetAmbientLight(Color(0.1f, 0.1f, 0.12f, 1));
            RenderSettings::SetDefaultReflectionMode(DefaultReflectionMode::Skybox);
            RenderSettings::SetSkybox(nullptr);
        }

        Material* Lit(Color color) { return scene.Colored("Universal Render Pipeline/Lit", color); }
        Material* SimpleLit(Color color) { return scene.Colored("Universal Render Pipeline/Simple Lit", color); }
    };

    // The red channel of a pixel.
    u32 Red(u32 pixel) { return pixel & 0xFFu; }

    // The fields of a note.
    Array<String> Fields(StringView note)
    {
        Array<String> fields;
        usize start = 0;
        for (usize i = 0; i <= note.Size(); i++)
            if (i == note.Size() || note.Data()[i] == ':')
            {
                fields.Push(String(StringView(note.Data() + start, i - start)));
                start = i + 1;
            }
        return fields;
    }

    bool SameImage(const Array<u32>& a, const Array<u32>& b) { return a.Size() == b.Size() && std::memcmp(a.Data(), b.Data(), a.Size() * 4) == 0; }
}

HX_TEST(Standard, ShadowUtilsInBothLanguages)
{
    // Without a frame: tile sizes of atlases, a tile's transform, the fade and a light's bias. In a frame, a C++ and a
    // C# pass note the cascades as ShadowUtils makes them (1, 2 and 4 cascades, hard and soft): the same text, the
    // tiles side by side and in a square, larger spheres for later cascades, the bias 2.5 times larger with soft
    // shadows.
    HX_REQUIRE(StartScripts());
    HX_CHECK(ShadowUtils::GetMaxTileResolutionInAtlas(1024, 1024, 1) == 1024);
    HX_CHECK(ShadowUtils::GetMaxTileResolutionInAtlas(1024, 512, 2) == 512);
    HX_CHECK(ShadowUtils::GetMaxTileResolutionInAtlas(1024, 1024, 3) == 512);
    HX_CHECK(ShadowUtils::GetMaxTileResolutionInAtlas(1024, 1024, 4) == 512);
    HX_CHECK(ShadowUtils::GetMaxTileResolutionInAtlas(1024, 1024, 5) == 256);
    HX_CHECK(ShadowUtils::GetMaxTileResolutionInAtlas(0, 1024, 1) == 0);
    ShadowSliceData slice;
    slice.offsetX = 512, slice.offsetY = 256, slice.resolution = 256;
    ShadowUtils::ApplySliceTransform(slice, 1024, 512);
    Vector4 p = slice.shadowTransform * Vector4(0.5f, 0.5f, 0.25f, 1);
    HX_CHECK(p.x == 0.625f && p.y == 0.75f && p.z == 0.25f && p.w == 1.0f);
    f32 scale = 0, bias = 0;
    ShadowUtils::GetScaleAndBiasForLinearDistanceFade(400, 0.2f, scale, bias);
    HX_CHECK(std::fabs(256 * scale + bias) < 1e-5f && std::fabs(400 * scale + bias - 1) < 1e-5f);
    ShadowUtils::GetScaleAndBiasForLinearDistanceFade(400, 0, scale, bias);
    HX_CHECK(scale == 1000.0f && bias == -400000.0f);
    UniversalShadowData data;
    data.bias.Push(Vector4(1, 2, 0, 0));
    VisibleLight directional;
    Matrix4x4 projection = Matrix4x4::identity;
    projection(0, 0) = 0.2f;
    Vector4 b = ShadowUtils::GetShadowBias(directional, 0, data, projection, 512);
    HX_CHECK(std::fabs(b.x + 10.0f / 512) < 1e-6f && std::fabs(b.y + 20.0f / 512) < 1e-6f);
    HX_CHECK(ShadowUtils::GetShadowBias(directional, 1, data, projection, 512) == Vector4());

    GameObject* host = GameObject::Create("Values");
    host->AddComponent(CSharp("CsShadowUtilsValues").type);
    Notes().Clear();
    RunScriptPhase(ScriptMessage::Update);
    DestroyImmediate(host);
    HX_CHECK(SameNotes({"tiles:1024:512:512:512:256:0", "slice:0.625:0.75:0.25:1"}));

    u32 devices = ForEachDevice([&](rhi::Device& device, StringView) {
        ShaderFolder shaders;
        TestTarget target(device, 64, 48);
        ShadowScene s(Vector3(0, 3, -6), Vector3(20, 0, 0), Vector3(50, 150, 0), LightShadows::Hard, 30, 1024);
        s.scene.Thing("Ground", s.Lit(Color(0.8f, 0.8f, 0.8f, 1)), Vector3(0, -0.05f, 4), Quaternion::identity, Vector3(16, 0.1f, 20));
        s.scene.Thing("Block", s.Lit(Color(0.8f, 0.3f, 0.2f, 1)), Vector3(0, 0.5f, 0), Quaternion::identity, Vector3(1, 1, 1));
        auto* data = static_cast<UniversalRendererData*>(s.asset->rendererDataList()[0]);
        auto* cpp = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(scripting::TypeOf<CppShadowFeature>()));
        auto* cs = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(CSharp("CsShadowFeature").type));
        HX_REQUIRE(cpp && cs);
        s.scene.assets.Push(cpp), s.scene.assets.Push(cs);
        s.asset->SetSupportsSoftShadows(true);
        Array<ScriptableRendererFeature*> features;
        features.Push(cpp), features.Push(cs);
        data->SetRendererFeatures(features);

        struct Run
        {
            i32 cascades;
            LightShadows shadows;
            const char* atlas;
        };
        const Run runs[] = {{1, LightShadows::Hard, "atlas:1:1024:1024:1024"},
                            {2, LightShadows::Hard, "atlas:2:1024:512:512"},
                            {4, LightShadows::Hard, "atlas:4:1024:1024:512"},
                            {4, LightShadows::Soft, "atlas:4:1024:1024:512"}};
        Array<String> hard;
        for (const Run& run : runs)
        {
            s.asset->SetShadowCascadeCount(run.cascades);
            s.sun->SetShadows(run.shadows);
            Notes().Clear();
            CppNotes().Clear();
            HX_REQUIRE(target.Render());
            bool same = CppNotes().Size() == usize(run.cascades) + 1 && Notes().Size() == CppNotes().Size();
            for (usize i = 0; same && i < Notes().Size(); i++)
                same = Notes()[i] == CppNotes()[i].View();
            if (!same || CppNotes().Empty() || CppNotes()[0] != StringView(run.atlas))
            {
                for (const String& n : CppNotes())
                    std::printf("    cpp %s\n", n.CStr());
                for (const String& n : Notes())
                    std::printf("    cs  %s\n", n.CStr());
            }
            HX_CHECK(same);
            HX_REQUIRE(!CppNotes().Empty());
            HX_CHECK(CppNotes()[0] == StringView(run.atlas));
            // Per cascade: its tile, a sphere larger than the one before, the bias.
            i64 lastRadius = 0;
            for (i32 i = 0; i < run.cascades && usize(i) + 1 < CppNotes().Size(); i++)
            {
                Array<String> parts = Fields(CppNotes()[u32(i) + 1].View());
                HX_REQUIRE(parts.Size() == 15);
                i32 tile = run.cascades == 1 ? 1024 : 512;
                HX_CHECK(parts[2] == StringView("1"));
                HX_CHECK(parts[3] == Format("{}", (i % 2) * tile).View() && parts[4] == Format("{}", (i / 2) * tile).View());
                HX_CHECK(parts[5] == Format("{}", tile).View());
                i64 radius = std::atoll(parts[9].CStr());
                HX_CHECK(radius > lastRadius);
                lastRadius = radius;
            }
            if (run.cascades == 4 && run.shadows == LightShadows::Hard)
                hard = CppNotes();
        }
        // Soft shadows: the same cascades, the bias 2.5 times larger.
        HX_REQUIRE(hard.Size() == 5 && CppNotes().Size() == 5);
        for (u32 i = 1; i < 5; i++)
        {
            Array<String> h = Fields(hard[i].View()), soft = Fields(CppNotes()[i].View());
            HX_REQUIRE(h.Size() == 15 && soft.Size() == 15);
            f64 hardDepth = f64(std::atoll(h[13].CStr())), softDepth = f64(std::atoll(soft[13].CStr()));
            HX_CHECK(hardDepth < 0 && std::fabs(softDepth / hardDepth - 2.5) < 0.01);
            for (u32 k = 0; k < 13; k++)
                HX_CHECK(h[k].View() == soft[k].View());
        }
        Notes().Clear();
        CppNotes().Clear();
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, CascadedShadowsOnEveryDevice)
{
    // A row of pillars from the camera to beyond the shadow distance under a low sun, with 1 to 4 cascades: the
    // shadows near the camera sharper with more cascades, faded out over the last part of the distance, none beyond.
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 128, height = 72;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        ShadowScene s(Vector3(0, 2.5f, -2), Vector3(12, 0, 0), Vector3(35, -50, 0), LightShadows::Hard, 25, 1024);
        s.scene.Thing("Ground", s.SimpleLit(Color(0.8f, 0.8f, 0.8f, 1)), Vector3(0, -0.05f, 22), Quaternion::identity, Vector3(20, 0.1f, 50));
        Material* pillar = s.Lit(Color(0.9f, 0.4f, 0.3f, 1));
        for (i32 j = 0; j < 10; j++)
            s.scene.Thing("Pillar", pillar, Vector3(j % 2 ? 1.6f : -1.6f, 1, 1 + 3.0f * f32(j)), Quaternion::identity, Vector3(0.4f, 2, 0.4f));
        Array<u32> images[4];
        for (i32 cascades = 1; cascades <= 4; cascades++)
        {
            s.asset->SetShadowCascadeCount(cascades);
            HX_REQUIRE(target.Render());
            HX_CHECK(target.Matches(Format("StandardCascades{}", cascades).CStr(), label));
            images[cascades - 1] = target.pixels;
        }
        // More cascades: other shadows near the camera.
        HX_CHECK(!SameImage(images[0], images[3]));
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, SoftShadowTiersOnEveryDevice)
{
    // A block's shadow up close: hard, then soft at the asset's low, medium and high quality (wider, smoother edges);
    // a light's own quality over the asset's.
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        ShadowScene s(Vector3(0, 2.2f, -2.4f), Vector3(40, 0, 0), Vector3(55, 30, 0), LightShadows::Hard, 8, 512);
        s.scene.Thing("Ground", s.SimpleLit(Color(0.8f, 0.8f, 0.8f, 1)), Vector3(0, -0.05f, 1), Quaternion::identity, Vector3(10, 0.1f, 10));
        s.scene.Thing("Block", s.Lit(Color(0.3f, 0.5f, 0.9f, 1)), Vector3(0, 0.5f, 0.3f), Quaternion::Euler(0, 30, 0), Vector3(1, 1, 1));
        s.asset->SetSupportsSoftShadows(true);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardSoftHard", label));
        Array<u32> hardImage = target.pixels;
        s.sun->SetShadows(LightShadows::Soft);
        struct Tier
        {
            SoftShadowQuality quality;
            const char* golden;
        };
        const Tier tiers[] = {{SoftShadowQuality::Low, "StandardSoftLow"}, {SoftShadowQuality::Medium, "StandardSoftMedium"},
                              {SoftShadowQuality::High, "StandardSoftHigh"}};
        Array<u32> images[3];
        u32 k = 0;
        for (const Tier& tier : tiers)
        {
            s.asset->SetSoftShadowQuality(tier.quality);
            HX_REQUIRE(target.Render());
            HX_CHECK(target.Matches(tier.golden, label));
            images[k++] = target.pixels;
        }
        HX_CHECK(!SameImage(images[0], hardImage) && !SameImage(images[1], images[0]) && !SameImage(images[2], images[1]));
        // The light's own quality (high) over the asset's (low).
        s.asset->SetSoftShadowQuality(SoftShadowQuality::Low);
        UniversalAdditionalLightData* extra = GetUniversalAdditionalLightData(s.sun);
        HX_REQUIRE(extra != nullptr);
        extra->SetSoftShadowQuality(SoftShadowQuality::High);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardSoftHigh", label));
        // Soft shadows off in the asset: hard again.
        extra->SetSoftShadowQuality(SoftShadowQuality::UsePipelineSettings);
        s.asset->SetSupportsSoftShadows(false);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardSoftHard", label));
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, ShadowBiasKeepsALitGroundCleanOnEveryDevice)
{
    // A ground under a low sun shadows itself: with the depth and normal bias every pixel of it outside the block's
    // shadow is lit the same (no acne), the block's shadow is there.
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        ShadowScene s(Vector3(0, 1.5f, -3), Vector3(15, 0, 0), Vector3(12, 20, 0), LightShadows::Hard, 20, 1024);
        s.scene.Thing("Ground", s.SimpleLit(Color(0.8f, 0.8f, 0.8f, 1)), Vector3(0, -0.05f, 10), Quaternion::identity, Vector3(20, 0.1f, 30));
        // A block on the left: its long shadow falls on the left half, the right half is the ground's own shadow only.
        s.scene.Thing("Block", s.SimpleLit(Color(0.8f, 0.8f, 0.8f, 1)), Vector3(-2.5f, 0.5f, 1), Quaternion::identity, Vector3(1, 1, 1));
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardNoAcne", label));
        // The ground's red channel on the right half (the background has none): one lit value, no darker specks.
        u32 low = 255, high = 0, count = 0;
        for (u32 y = 0; y < height; y++)
            for (u32 x = width / 2 + 4; x < width; x++)
                if (u32 r = Red(target.pixels[y * width + x]); r > 0)
                    low = Min(low, r), high = Max(high, r), count++;
        HX_CHECK(count > width * height / 8 && high - low <= 2);
        if (high - low > 2)
            std::printf("    the lit ground spreads from %u to %u\n", low, high);
        // The block's shadow on the left half: darker than the lit ground.
        u32 shadowed = 0;
        for (u32 y = 0; y < height; y++)
            for (u32 x = 0; x < width / 2; x++)
                if (u32 r = Red(target.pixels[y * width + x]); r > 0 && r + 15 < low)
                    shadowed++;
        HX_CHECK(shadowed > 20);
    });
    std::printf("    %u devices\n", devices);
}
