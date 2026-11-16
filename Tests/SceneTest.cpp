#include <Support/TestScene.h>

#include <Cpp/CppFeatures.h>

#include <Helix/Engine/Rendering/RenderSettings.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::engine;
using namespace helix::rendering::universal;
using namespace helix::rpstandard::testing;

HX_TEST(Standard, TheFrameOfATestSceneOnEveryDevice)
{
    // The test scene with a C++ feature's pass after the opaques and a C# one's before the transparents: drawn
    // straight into the camera's target (Auto at render scale 1), through the intermediate texture at render scale 0.5
    // (drawn smaller, blitted up), with the Always mode and with a pass that requires the intermediate texture (the
    // same image as straight).
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        TestScene scene;
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
        auto* cppFeature = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(scripting::TypeOf<CppTestFeature>()));
        auto* csFeature = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(CSharp("CsTestFeature").type));
        HX_REQUIRE(cppFeature && csFeature);
        scene.assets.Push(cppFeature), scene.assets.Push(csFeature), scene.assets.Push(data), scene.assets.Push(asset);
        Array<ScriptableRendererFeature*> features;
        features.Push(cppFeature), features.Push(csFeature);
        data->SetRendererFeatures(features);
        asset->SetShadowDistance(30);
        asset->SetMainLightShadowmapResolution(1024);
        // An LDR camera color: the camera's target takes it (HDR needs the intermediate texture).
        asset->SetSupportsHDR(false);
        GraphicsSettings::SetDefaultRenderPipeline(asset);

        struct Run
        {
            f32 scale;
            IntermediateTextureMode mode;
            bool passNeedsIntermediate;
            const char* golden;
            const char* target;
        };
        const Run runs[] = {{1.0f, IntermediateTextureMode::Auto, false, "StandardScene", "back"},
                            {0.5f, IntermediateTextureMode::Auto, false, "StandardSceneHalf", "texture"},
                            {1.0f, IntermediateTextureMode::Always, false, "StandardScene", "texture"},
                            {1.0f, IntermediateTextureMode::Auto, true, "StandardScene", "texture"}};
        for (const Run& run : runs)
        {
            asset->SetRenderScale(run.scale);
            data->SetIntermediateTextureMode(run.mode);
            CppPassRequiresIntermediate() = run.passNeedsIntermediate;
            Notes().Clear();
            CppNotes().Clear();
            HX_REQUIRE(target.Render());
            i32 scaled = i32(std::lround(width * run.scale));
            String cpp = Format("cpp:300:{}:color:{}", scaled, run.target), cs = Format("cs:450:{}:color:True", scaled);
            bool passes = CppNotes().Size() == 1 && CppNotes()[0] == cpp.View() && Notes().Size() == 1 && Notes()[0] == cs.View();
            if (!passes)
            {
                for (const String& n : CppNotes())
                    std::printf("    %s\n", n.CStr());
                for (const String& n : Notes())
                    std::printf("    %s\n", n.CStr());
            }
            HX_CHECK(passes);
            HX_CHECK(target.Matches(run.golden, label));
        }
        CppPassRequiresIntermediate() = false;
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, TheCopiesOfDepthAndColorOnEveryDevice)
{
    // _CameraDepthTexture and _CameraOpaqueTexture read by a transparent pane over the scene: the depth copied after
    // the opaques, after the transparents and drawn by a depth prepass (the same depth), the opaque color at each
    // downsampling.
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        TestScene scene;
        Material* reader = Material::Create(Shader::Find("Tests/StandardReadCopies"));
        scene.assets.Push(reader);
        scene.Thing("Reader", reader, Vector3(0, 2, -3), Quaternion::Euler(22, 0, 0), Vector3(5, 3, 0.01f));
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
        scene.assets.Push(data), scene.assets.Push(asset);
        asset->SetShadowDistance(30);
        asset->SetMainLightShadowmapResolution(1024);
        asset->SetSupportsCameraDepthTexture(true);
        asset->SetSupportsCameraOpaqueTexture(true);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        struct Run
        {
            CopyDepthMode depth;
            Downsampling color;
            const char* golden;
        };
        const Run runs[] = {{CopyDepthMode::AfterOpaques, Downsampling::None, "StandardCopiesNone"},
                            {CopyDepthMode::AfterTransparents, Downsampling::_2xBilinear, "StandardCopies2xBilinear"},
                            {CopyDepthMode::ForcePrepass, Downsampling::_4xBox, "StandardCopies4xBox"},
                            {CopyDepthMode::AfterOpaques, Downsampling::_4xBilinear, "StandardCopies4xBilinear"}};
        for (const Run& run : runs)
        {
            data->SetCopyDepthMode(run.depth);
            asset->SetOpaqueDownsampling(run.color);
            HX_REQUIRE(target.Render());
            HX_CHECK(target.Matches(run.golden, label));
        }
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, MultisampledFramesOnEveryDevice)
{
    // The asset's 4x MSAA: the test scene's edges blended (only they differ from the frame without it), and a pane
    // reading _CameraDepthTexture and _CameraOpaqueTexture under MSAA (the depth from the prepass, the color resolved).
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        TestScene scene;
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        scene.assets.Push(asset->rendererDataList()[0]), scene.assets.Push(asset);
        asset->SetShadowDistance(30);
        asset->SetMainLightShadowmapResolution(1024);
        asset->SetSupportsHDR(false);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        HX_REQUIRE(target.Render());
        Array<u32> single = target.pixels;
        asset->SetMsaaSampleCount(4);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardSceneMsaa", label));
        u32 differing = 0;
        for (usize i = 0; i < single.Size(); i++)
            differing += single[i] != target.pixels[i] ? 1 : 0;
        HX_CHECK(differing > 20 && differing < width * height / 5);

        Material* reader = Material::Create(Shader::Find("Tests/StandardReadCopies"));
        scene.assets.Push(reader);
        scene.Thing("Reader", reader, Vector3(0, 2, -3), Quaternion::Euler(22, 0, 0), Vector3(5, 3, 0.01f));
        asset->SetSupportsCameraDepthTexture(true);
        asset->SetSupportsCameraOpaqueTexture(true);
        asset->SetOpaqueDownsampling(Downsampling::None);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardCopiesMsaa", label));
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, TheSkyboxBehindTheSceneOnEveryDevice)
{
    // The test scene's camera clears to the skybox (Skybox/6 Sided, a gradient per side): drawn after the opaques,
    // behind them. The same image when the camera clears to a color and a C# pass draws the C# graph's skybox list.
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        TestScene scene;
        Material* sky = Material::Create(Shader::Find("Skybox/6 Sided"));
        HX_REQUIRE(sky != nullptr);
        scene.assets.Push(sky);
        const char* sides[6] = {"_FrontTex", "_BackTex", "_LeftTex", "_RightTex", "_UpTex", "_DownTex"};
        for (u32 f = 0; f < 6; f++)
        {
            f32 hue = f32(f) / 5.0f;
            Texture2D* side = TestTexture(32, false, [hue](f32 x, f32 y) { return Color(0.2f + 0.6f * y, 0.3f + 0.4f * hue, 0.9f - 0.5f * y + 0.1f * x, 1); });
            side->SetWrapMode(TextureWrapMode::Clamp);
            scene.assets.Push(side);
            sky->SetTexture(sides[f], side);
        }
        RenderSettings::SetSkybox(sky);
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
        scene.assets.Push(data), scene.assets.Push(asset);
        asset->SetShadowDistance(30);
        asset->SetMainLightShadowmapResolution(1024);
        asset->SetSupportsHDR(false);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        scene.camera->SetClearFlags(CameraClearFlags::Skybox);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardSkybox", label));

        auto* csSky = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(CSharp("CsSkyFeature").type));
        HX_REQUIRE(csSky != nullptr);
        scene.assets.Push(csSky);
        Array<ScriptableRendererFeature*> features;
        features.Push(csSky);
        data->SetRendererFeatures(features);
        scene.camera->SetClearFlags(CameraClearFlags::SolidColor);
        HX_REQUIRE(target.Render());
        HX_CHECK(target.Matches("StandardSkybox", label));
        RenderSettings::SetSkybox(nullptr);
    });
    std::printf("    %u devices\n", devices);
}
