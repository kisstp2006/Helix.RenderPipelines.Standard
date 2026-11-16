#include <Support/TestScene.h>

#include <Helix/Engine/Rendering/SystemInfo.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Testing/Test.h>

#include <cstdlib>

using namespace helix;
using namespace helix::engine;
using namespace helix::rendering::universal;
using namespace helix::rpstandard::testing;

namespace
{
    // An asset whose renderer data is the C# test class's.
    UniversalRenderPipelineAsset* CsRendererAsset(TestScene& scene, ScriptableRendererData*& data)
    {
        data = static_cast<ScriptableRendererData*>(ScriptableObject::CreateInstance(CSharp("CsRendererData").type));
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create(data);
        scene.assets.Push(data), scene.assets.Push(asset);
        asset->SetShadowDistance(30);
        asset->SetSupportsHDR(false);
        return asset;
    }
}

HX_TEST(Standard, ARendererOfCSharpOnEveryDevice)
{
    // A C# renderer data makes a C# renderer: its culling override drops layer 3 (the green box) and the shadows, its
    // own pass draws the scene with the pipeline's C# helpers; a renderer the device does not take draws nothing.
    HX_REQUIRE(StartScripts());
    constexpr u32 width = 96, height = 64;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        TestTarget target(device, width, height);
        TestScene scene;
        GameObject* unlit = GameObject::Find("Unlit");
        HX_REQUIRE(unlit != nullptr);
        unlit->SetLayer(3);
        ScriptableRendererData* data = nullptr;
        UniversalRenderPipelineAsset* asset = CsRendererAsset(scene, data);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        ScriptableRenderer* renderer = asset->scriptableRenderer();
        HX_REQUIRE(renderer != nullptr && renderer->HelixType() != scripting::TypeOf<UniversalRenderer>() && renderer->data() == data);
        Notes().Clear();
        HX_REQUIRE(target.Render());
        HX_CHECK(SameNotes({"cull:Eye:FFFFFFF7", "draw:96:True:False:True:True"}));
        HX_CHECK(target.Matches("StandardCsRenderer", label));
        // The same renderer again (the asset keeps it, and its C# object).
        HX_CHECK(asset->scriptableRenderer() == renderer);

        // Not on this device: nothing drawn (the target keeps what it had).
        Notes().Clear();
        GameObject* host = GameObject::Create("Unsupported");
        host->AddComponent(CSharp("CsUnsupportedDevice").type);
        RunScriptPhase(ScriptMessage::Update);
        DestroyImmediate(host);
        HX_REQUIRE(target.Render());
        HX_CHECK(Notes().Empty());
        GameObject* reset = GameObject::Create("Supported");
        reset->AddComponent(CSharp("CsSupportedDevices").type);
        RunScriptPhase(ScriptMessage::Update);
        DestroyImmediate(reset);
        Notes().Clear();
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, PassesOfCSharpGoWithTheirObjects)
{
    // A C# feature queues a new C# pass each frame and drops it: once collected, its native object is deleted before
    // the next frame (the renderer kept it alive while queued); the feature itself lives on.
    HX_REQUIRE(StartScripts());
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView) {
        ShaderFolder shaders;
        TestTarget target(device, 32, 32);
        TestScene scene;
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
        auto* feature = static_cast<ScriptableRendererFeature*>(ScriptableObject::CreateInstance(CSharp("CsDropFeature").type));
        scene.assets.Push(feature), scene.assets.Push(data), scene.assets.Push(asset);
        Array<ScriptableRendererFeature*> features;
        features.Push(feature);
        data->SetRendererFeatures(features);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        Notes().Clear();
        HX_REQUIRE(target.Render());
        HX_CHECK(SameNotes({"dropped pass recorded"}));
        GameObject* host = GameObject::Create("Collect");
        host->AddComponent(CSharp("CsCollect").type);
        RunScriptPhase(ScriptMessage::Update);
        DestroyImmediate(host);
        // The first frame's pass: queued (and so alive) until the frame ended, deleted at the next frame's start.
        HX_REQUIRE(Notes().Size() == 1 && Notes()[0].View().StartsWith("collected:"));
        u64 first = std::strtoull(Notes()[0].CStr() + 10, nullptr, 10);
        Notes().Clear();
        HX_CHECK(first != 0 && ScriptableRenderPass::Find(first) != nullptr);
        HX_REQUIRE(target.Render());
        HX_CHECK(ScriptableRenderPass::Find(first) == nullptr);
        HX_CHECK(SameNotes({"dropped pass recorded"}));
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, TheGlobalSettingsComeBeforeThePipeline)
{
    // The asset's EnsureGlobalSettings registers UniversalRenderPipelineGlobalSettings (its default volume profile and
    // render graph settings) before the C++ pipeline is made; RenderPipelineManager.currentPipeline is the C# pipeline.
    HX_REQUIRE(StartScripts());
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView) {
        ShaderFolder shaders;
        TestTarget target(device, 16, 16);
        TestScene scene;
        UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
        scene.assets.Push(asset->rendererDataList()[0]), scene.assets.Push(asset);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        HX_REQUIRE(target.Render());
        Notes().Clear();
        GameObject* host = GameObject::Create("GlobalSettings");
        host->AddComponent(CSharp("CsGlobalSettingsTests").type);
        RunScriptPhase(ScriptMessage::Update);
        DestroyImmediate(host);
        HX_CHECK(SameNotes({"settings:True:True:0:True:True:0:True:True:True"}));
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(Standard, RendererDataRemakesItsRenderersWhenEnabledOrChanged)
{
    // OnEnable and OnValidate of C++ renderer data: a new version, so the asset makes its renderer again.
    HX_REQUIRE(StartScripts());
    UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
    auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
    ScriptableRenderer* first = asset->scriptableRenderer();
    u32 version = data->version();
    CallScriptMessage(data, data->script(), ScriptMessage::OnValidate);
    HX_CHECK(data->version() == version + 1);
    ScriptableRenderer* second = asset->scriptableRenderer();
    HX_CHECK(second != nullptr && second->dataVersion() == data->version() && ScriptableRenderer::Find(second->Id()) == second);
    (void)first;
    CallScriptMessage(data, data->script(), ScriptMessage::OnEnable);
    HX_CHECK(data->version() == version + 2);
    DestroyImmediate(asset);
    DestroyImmediate(data);
}
