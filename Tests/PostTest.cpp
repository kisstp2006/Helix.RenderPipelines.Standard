#include <Support/TestScene.h>

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

namespace
{
    u32 Channel(u32 pixel, u32 c) { return (pixel >> (8 * c)) & 0xFFu; }

    // The largest channel difference of two images.
    u32 MaxDifference(const Array<u32>& a, const Array<u32>& b)
    {
        u32 worst = 0;
        for (usize i = 0; i < a.Size() && i < b.Size(); i++)
            for (u32 c = 0; c < 3; c++)
                worst = Max(worst, u32(std::abs(i32(Channel(a[i], c)) - i32(Channel(b[i], c)))));
        return worst;
    }
}

HX_TEST(Standard, PostProcessingOnEveryDevice)
{
    // The test scene with an HDR bright box under a global volume, the camera's post processing on: an empty profile
    // (the identity look up table: the frame without post processing), bloom (and its high quality filtering), each
    // tonemapper, color adjustments, HDR grading with ACES; C# turning the saturation down at run time (gray).
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        TestScene scene;
        Material* bright = scene.Colored("Universal Render Pipeline/Unlit", Color(3.0f, 2.2f, 0.8f, 1));
        scene.Thing("Bright", bright, Vector3(2.4f, 1.3f, 1.2f), Quaternion::Euler(0, 20, 0), Vector3(0.7f, 0.7f, 0.7f));
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        scene.assets.Push(asset->rendererDataList()[0]), scene.assets.Push(asset);
        asset->SetShadowDistance(30);
        asset->SetMainLightShadowmapResolution(1024);
        asset->SetSupportsHDR(true);
        GraphicsSettings::SetDefaultRenderPipeline(asset);

        auto* profile = static_cast<VolumeProfile*>(ScriptableObject::CreateInstance(scripting::TypeOf<VolumeProfile>()));
        HX_REQUIRE(profile != nullptr);
        scene.assets.Push(profile);
        GameObject* volumeHolder = GameObject::Create("PostVolume");
        scene.made.Push(volumeHolder);
        // A script class of the core: added by its type.
        auto* volume = static_cast<Volume*>(volumeHolder->AddComponent(scripting::TypeOf<Volume>()));
        HX_REQUIRE(volume != nullptr);
        volume->SetIsGlobal(true);
        volume->SetSharedProfile(profile);
        Bloom* bloom = profile->Add<Bloom>();
        Tonemapping* tonemapping = profile->Add<Tonemapping>();
        ColorAdjustments* adjustments = profile->Add<ColorAdjustments>();
        HX_REQUIRE(bloom && tonemapping && adjustments);

        // Without post processing, then with the empty profile: the same image.
        HX_REQUIRE(target.Render());
        Array<u32> plain = target.pixels;
        GetUniversalAdditionalCameraData(scene.camera)->SetRenderPostProcessing(true);
        HX_REQUIRE(target.Render());
        u32 identity = MaxDifference(plain, target.pixels);
        HX_CHECK(identity <= 2);
        if (identity > 2)
            std::printf("    the identity table differs by %u\n", identity);

        struct Run
        {
            const char* golden;
            void (*set)(Bloom&, Tonemapping&, ColorAdjustments&, UniversalRenderPipelineAsset&);
        };
        const Run runs[] = {
            {"StandardBloom", [](Bloom& b, Tonemapping&, ColorAdjustments&, UniversalRenderPipelineAsset&) { b.intensity.Override(1.0f), b.threshold.Override(1.0f); }},
            {"StandardBloomHQ", [](Bloom& b, Tonemapping&, ColorAdjustments&, UniversalRenderPipelineAsset&) { b.highQualityFiltering.Override(true); }},
            {"StandardTonemapNeutral",
             [](Bloom& b, Tonemapping& t, ColorAdjustments&, UniversalRenderPipelineAsset&) {
                 b.intensity.Override(0.0f);
                 t.mode.Override(TonemappingMode::Neutral);
             }},
            {"StandardTonemapACES", [](Bloom&, Tonemapping& t, ColorAdjustments&, UniversalRenderPipelineAsset&) { t.mode.Override(TonemappingMode::ACES); }},
            {"StandardColorAdjustments",
             [](Bloom&, Tonemapping& t, ColorAdjustments& c, UniversalRenderPipelineAsset&) {
                 t.mode.Override(TonemappingMode::None);
                 c.postExposure.Override(0.5f);
                 c.contrast.Override(20.0f);
                 c.colorFilter.Override(Color(1.0f, 0.9f, 0.8f, 1));
                 c.hueShift.Override(30.0f);
                 c.saturation.Override(-30.0f);
             }},
            {"StandardHdrGradingACES",
             [](Bloom& b, Tonemapping& t, ColorAdjustments& c, UniversalRenderPipelineAsset& a) {
                 c.SetAllOverridesTo(false);
                 b.intensity.Override(0.6f);
                 t.mode.Override(TonemappingMode::ACES);
                 a.SetColorGradingMode(ColorGradingMode::HighDynamicRange);
             }},
        };
        Array<u32> previous = target.pixels;
        for (const Run& run : runs)
        {
            run.set(*bloom, *tonemapping, *adjustments, *asset);
            HX_REQUIRE(target.Render());
            HX_CHECK(target.Matches(run.golden, label));
            // Each setting changes the image.
            HX_CHECK(MaxDifference(previous, target.pixels) > 8);
            previous = target.pixels;
        }

        // From C#: the profile's components through their C# classes; saturation all the way down makes it gray.
        asset->SetColorGradingMode(ColorGradingMode::LowDynamicRange);
        Notes().Clear();
        GameObject* host = GameObject::Create("PostTweaks");
        host->AddComponent(CSharp("CsPostTweaks").type);
        RunScriptPhase(ScriptMessage::Update);
        DestroyImmediate(host);
        HX_CHECK(SameNotes({"data:Hidden/Universal Render Pipeline/Bloom:Hidden/Universal Render Pipeline/UberPost:Hidden/Universal Render Pipeline/LutBuilderHdr:True:0",
                            "post:True:True:True:0.6:ACES:TonemappingModeParameter:Half:DownscaleParameter:10:1:5:-100"}));
        HX_REQUIRE(target.Render());
        u32 colorful = 0;
        for (u32 pixel : target.pixels)
        {
            u32 r = Channel(pixel, 0), g = Channel(pixel, 1), b = Channel(pixel, 2);
            colorful = Max(colorful, Max(Max(r, g), b) - Min(Min(r, g), b));
        }
        HX_CHECK(colorful <= 3);
        if (colorful > 3)
            std::printf("    gray image has a chroma of %u\n", colorful);

        // Renderer data without post processing data: no post processing (the plain frame again).
        auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
        HX_REQUIRE(data->postProcessData() == PostProcessData::Default());
        data->SetPostProcessData(nullptr);
        HX_REQUIRE(target.Render());
        HX_CHECK(MaxDifference(plain, target.pixels) == 0);
        data->SetPostProcessData(PostProcessData::Default());
    });
    std::printf("    %u devices\n", devices);
}
