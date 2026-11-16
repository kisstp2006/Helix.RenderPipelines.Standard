#include <Support/StandardHost.h>

#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/Asset/Resources.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Scene/SceneFile.h>
#include <Helix/Engine/Scene/SceneManager.h>
#include <Helix/Engine/Script/MonoScript.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/RenderPipelines/Standard/Components/AdditionalData.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipeline.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::engine;
using namespace helix::rendering::universal;
using namespace helix::rpstandard::testing;

namespace
{
    // The engine's object of a script class (a C++ one here) as an asset of a guid, for files that name it.
    MonoScript* RegisterScript(scripting::TypeId type, const char* guidText)
    {
        Guid guid;
        Guid::TryParse(guidText, guid);
        MonoScript* script = MonoScript::FromScriptClass(*FindScriptClass(type));
        RegisterAssetObject(guid, 11500000, script, true);
        return script;
    }
}

HX_TEST(Standard, TheAssetKeepsItsSettingsWithinTheirLimits)
{
    // A new asset with its renderer data, the reference's defaults, the setters' limits, the renderer made once per
    // data version.
    HX_REQUIRE(StartScripts());
    UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
    HX_REQUIRE(asset != nullptr && asset->rendererDataList().Size() == 1);
    auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
    HX_REQUIRE(data != nullptr);
    HX_CHECK(asset->renderScale() == 1 && asset->msaaSampleCount() == 1 && asset->shadowDistance() == 50 && asset->shadowCascadeCount() == 1 &&
             asset->mainLightShadowmapResolution() == 2048 && asset->maxAdditionalLightsCount() == 4 && asset->supportsHDR() &&
             asset->colorGradingLutSize() == 32 && asset->numIterationsEnclosingSphere() == 64 && asset->useSRPBatcher());
    asset->SetRenderScale(5);
    asset->SetMsaaSampleCount(3);
    asset->SetShadowCascadeCount(7);
    asset->SetColorGradingLutSize(100);
    asset->SetMainLightShadowmapResolution(3000);
    asset->SetMaxAdditionalLightsCount(20);
    asset->SetShadowDepthBias(-1);
    HX_CHECK(asset->renderScale() == 2 && asset->msaaSampleCount() == 2 && asset->shadowCascadeCount() == 4 && asset->colorGradingLutSize() == 65 &&
             asset->mainLightShadowmapResolution() == 4096 && asset->maxAdditionalLightsCount() == 8 && asset->shadowDepthBias() == 0);
    ScriptableRenderer* renderer = asset->scriptableRenderer();
    HX_CHECK(renderer != nullptr && renderer->data() == data && renderer == asset->GetRenderer(0) && asset->GetRenderer(1) == nullptr);
    data->SetDirty();
    HX_CHECK(asset->GetRenderer(0) != nullptr && asset->GetRenderer(0)->dataVersion() == data->version());
    HX_CHECK(data->opaqueLayerMaskBits() == -1 && data->shadowTransparentReceive() && data->renderingMode() == RenderingMode::Forward);
    // The pipeline's asset is this one while it is set.
    HX_CHECK(UniversalRenderPipeline::asset() == nullptr);
    GraphicsSettings::SetDefaultRenderPipeline(asset);
    HX_CHECK(UniversalRenderPipeline::asset() == asset);
    GraphicsSettings::SetDefaultRenderPipeline(nullptr);
    DestroyImmediate(asset);
    DestroyImmediate(data);
}

HX_TEST(Standard, SettingsInFiles)
{
    // The asset and its renderer data written into one file and read again: the reference's field names; a camera's
    // additional data in a scene.
    HX_REQUIRE(StartScripts());
    String root = platform::JoinPath(platform::TempDirectory(), Format("helix-standard-files-{}", Guid::Generate().ToString()));
    platform::CreateDirectories(platform::JoinPath(root, "Resources"));
    Array<MonoScript*> scripts;
    scripts.Push(RegisterScript(scripting::TypeOf<UniversalRenderPipelineAsset>(), "d0000000000000000000000000000001"));
    scripts.Push(RegisterScript(scripting::TypeOf<UniversalRendererData>(), "d0000000000000000000000000000002"));
    scripts.Push(RegisterScript(scripting::TypeOf<UniversalAdditionalCameraData>(), "d0000000000000000000000000000003"));

    UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
    auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
    asset->SetName("StandardAsset");
    data->SetName("StandardRenderer");
    asset->SetRenderScale(1.5f);
    asset->SetShadowCascadeCount(3);
    asset->SetCascade3Split(Vector2(0.2f, 0.5f));
    data->SetOpaqueLayerMaskBits(5);
    data->SetCopyDepthMode(CopyDepthMode::AfterTransparents);
    StencilStateData stencil;
    stencil.overrideStencilState = true;
    stencil.stencilReference = 3;
    data->SetDefaultStencilState(stencil);
    Array<u8> bytes;
    String error;
    Object* objects[] = {asset, data};
    HX_REQUIRE(WriteAssetFile(Span<Object* const>(objects, 2), bytes, error));
    StringView text(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    bool named = text.Contains("m_RenderScale: 1.5") && text.Contains("m_ShadowCascadeCount: 3") && text.Contains("m_RendererDataList:") &&
                 text.Contains("m_Cascade3Split: {x: 0.2, y: 0.5}") && text.Contains("m_OpaqueLayerMask:\n    serializedVersion: 2\n    m_Bits: 5") &&
                 text.Contains("m_CopyDepthMode: 1") && text.Contains("overrideStencilState: 1") && text.Contains("stencilReference: 3");
    if (!named)
        std::printf("%.*s\n", int(text.Size()), text.Data());
    HX_CHECK(named);
    platform::WriteFile(platform::JoinPath(root, "Resources/Standard.asset"), Span<const u8>(bytes.Data(), bytes.Size()));
    DestroyImmediate(asset);
    DestroyImmediate(data);
    auto* source = new FolderAssetSource(root);
    AddAssetSource(source);
    auto* read = static_cast<UniversalRenderPipelineAsset*>(Resources::Load("Standard"));
    HX_REQUIRE(read != nullptr && read->Class().type == scripting::TypeOf<UniversalRenderPipelineAsset>());
    Array<ScriptableRendererData*> list = read->rendererDataList();
    auto* readData = list.Size() == 1 ? static_cast<UniversalRendererData*>(list[0]) : nullptr;
    HX_CHECK(read->renderScale() == 1.5f && read->shadowCascadeCount() == 3 && read->cascade3Split().y == 0.5f);
    HX_CHECK(readData && readData->opaqueLayerMaskBits() == 5 && readData->copyDepthMode() == CopyDepthMode::AfterTransparents &&
             readData->defaultStencilState().overrideStencilState && readData->defaultStencilState().stencilReference == 3);
    HX_CHECK(readData && read->GetRenderer(0) && read->GetRenderer(0)->data() == readData);

    // A camera's additional data in a scene, read back.
    Scene start = SceneManager::GetActiveScene();
    Scene written = SceneManager::CreateScene("CameraData");
    SceneManager::SetActiveScene(written);
    GameObject* eye = GameObject::Create("Eye");
    Camera* camera = eye->AddComponent<Camera>();
    UniversalAdditionalCameraData* extra = GetUniversalAdditionalCameraData(camera);
    HX_REQUIRE(extra != nullptr && GetUniversalAdditionalCameraData(camera) == extra);
    extra->SetRenderShadows(false);
    extra->SetRequiresDepthTexture(true);
    extra->SetRenderer(1);
    extra->SetVolumeLayerMaskBits(6);
    extra->SetRenderPostProcessing(true);
    String yaml;
    HX_REQUIRE(WriteSceneYaml(written, yaml, error));
    Scene readScene = SceneManager::CreateScene("CameraDataRead");
    Array<String> errors;
    HX_REQUIRE(ReadSceneFile(StringView(yaml), readScene, errors));
    Array<GameObject*> roots = readScene.GetRootGameObjects();
    auto* back = roots.Size() == 1 ? static_cast<UniversalAdditionalCameraData*>(roots[0]->GetComponent(scripting::TypeOf<UniversalAdditionalCameraData>()))
                                   : nullptr;
    bool scene = yaml.View().Contains("m_RenderShadows: 0") && yaml.View().Contains("m_RequiresDepthTextureOption: 1") &&
                 yaml.View().Contains("m_RendererIndex: 1") && back && !back->renderShadows() && back->requiresDepthTexture() && back->rendererIndex() == 1 &&
                 back->volumeLayerMaskBits() == 6 && back->renderPostProcessing();
    if (!scene)
        std::printf("%.*s\n", int(yaml.Size()), yaml.Data());
    HX_CHECK(scene);
    SceneManager::SetActiveScene(start);
    SceneManager::UnloadSceneAsync(written);
    SceneManager::UnloadSceneAsync(readScene);
    SceneManager::ProcessSceneLoads();

    ResetAssets();
    RemoveAssetSource(source);
    delete source;
    for (MonoScript* s : scripts)
        DestroyImmediate(s, true);
    platform::RemoveFile(platform::JoinPath(root, "Resources/Standard.asset"));
    platform::RemoveEmptyDirectory(platform::JoinPath(root, "Resources"));
    platform::RemoveEmptyDirectory(root);
    engine::testing::Cleanup();
}

namespace
{
    // The text with its first `what` replaced by `with` (false when it has none).
    bool ReplaceFirst(String& text, StringView what, StringView with)
    {
        StringView v = text.View();
        usize at = v.Find(what);
        if (at == StringView::npos)
            return false;
        String out(v.Substr(0, at));
        out += with;
        out += v.Substr(at + what.Size());
        text = Move(out);
        return true;
    }
}

HX_TEST(Standard, OlderAssetFilesAreUpgraded)
{
    // An asset file of version 1 (one renderer data in m_RendererData, the cascades as an option of two or four):
    // read, its renderer data becomes the list and the option the cascade count; written again at this version.
    HX_REQUIRE(StartScripts());
    String root = platform::JoinPath(platform::TempDirectory(), Format("helix-standard-upgrade-{}", Guid::Generate().ToString()));
    platform::CreateDirectories(platform::JoinPath(root, "Resources"));
    Array<MonoScript*> scripts;
    scripts.Push(RegisterScript(scripting::TypeOf<UniversalRenderPipelineAsset>(), "d0000000000000000000000000000011"));
    scripts.Push(RegisterScript(scripting::TypeOf<UniversalRendererData>(), "d0000000000000000000000000000012"));
    UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
    auto* data = static_cast<UniversalRendererData*>(asset->rendererDataList()[0]);
    asset->SetName("OldAsset");
    data->SetName("OldRenderer");
    Array<u8> bytes;
    String error;
    Object* objects[] = {asset, data};
    HX_REQUIRE(WriteAssetFile(Span<Object* const>(objects, 2), bytes, error));
    String text(StringView(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    // The list's entry moves to m_RendererData; version 1; the option of four cascades.
    StringView v = text.View();
    usize list = v.Find("m_RendererDataList:\n");
    usize entry = list == StringView::npos ? StringView::npos : v.Find("- {fileID: ", list);
    usize end = entry == StringView::npos ? StringView::npos : v.Find("}", entry);
    HX_REQUIRE(end != StringView::npos);
    String reference(v.Substr(entry + 2, end + 1 - (entry + 2)));
    usize lineStart = entry;
    while (lineStart > 0 && v[lineStart - 1] != '\n')
        lineStart--;
    String entryLine(v.Substr(lineStart, end + 2 - lineStart));
    bool edited = ReplaceFirst(text, entryLine.View(), "") && ReplaceFirst(text, "m_RendererDataList:\n", "m_RendererDataList: []\n") &&
                  ReplaceFirst(text, "m_RendererData: {fileID: 0}", Format("m_RendererData: {}", reference).View()) &&
                  ReplaceFirst(text, "k_AssetVersion: 12", "k_AssetVersion: 1") && ReplaceFirst(text, "m_ShadowCascades: 0", "m_ShadowCascades: 2");
    if (!edited)
        std::printf("%s\n", text.CStr());
    HX_REQUIRE(edited);
    platform::WriteFile(platform::JoinPath(root, "Resources/OldStandard.asset"), Span<const u8>(reinterpret_cast<const u8*>(text.Data()), text.Size()));
    DestroyImmediate(asset);
    DestroyImmediate(data);
    auto* source = new FolderAssetSource(root);
    AddAssetSource(source);
    auto* read = static_cast<UniversalRenderPipelineAsset*>(Resources::Load("OldStandard"));
    HX_REQUIRE(read != nullptr);
    Array<ScriptableRendererData*> readList = read->rendererDataList();
    HX_CHECK(readList.Size() == 1 && readList[0] && readList[0]->Name() == StringView("OldRenderer") && read->shadowCascadeCount() == 4);
    Object* again[] = {read};
    Array<u8> rewritten;
    HX_REQUIRE(WriteAssetFile(Span<Object* const>(again, 1), rewritten, error));
    StringView now(reinterpret_cast<const char*>(rewritten.Data()), rewritten.Size());
    HX_CHECK(now.Contains("k_AssetVersion: 12") && now.Contains("k_AssetPreviousVersion: 1") && now.Contains("m_ShadowCascadeCount: 4"));
    ResetAssets();
    RemoveAssetSource(source);
    delete source;
    for (MonoScript* s : scripts)
        DestroyImmediate(s, true);
    platform::RemoveFile(platform::JoinPath(root, "Resources/OldStandard.asset"));
    platform::RemoveEmptyDirectory(platform::JoinPath(root, "Resources"));
    platform::RemoveEmptyDirectory(root);
    engine::testing::Cleanup();
}

HX_TEST(Standard, SettingsInCSharp)
{
    // The C# side: the asset as a RenderPipelineAsset, its constants, the renderer data's feature list and masks, the
    // camera and light extensions.
    HX_REQUIRE(StartScripts());
    Notes().Clear();
    GameObject* host = GameObject::Create("CsAssetHost");
    host->AddComponent(CSharp("CsAssetTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(SameNotes({"asset:True:True:UniversalPipeline:UniversalRenderPipeline:16:65:1:2",
                        "data:1:Forward:-1:5",
                        "features:0:1:True:False",
                        "pipeline:True:8:0.1:2:256",
                        "camera:True:True:False:Base:1:True",
                        "light:True:True:UsePipelineSettings"}));
    DestroyImmediate(host);
    engine::testing::Cleanup();
}

HX_TEST(Standard, PostProcessDataInFiles)
{
    // Post processing data in a file with two textures: the reference's nested shaders and textures fields, the
    // texture list and the single texture read back.
    HX_REQUIRE(StartScripts());
    String root = platform::JoinPath(platform::TempDirectory(), Format("helix-standard-postdata-{}", Guid::Generate().ToString()));
    platform::CreateDirectories(platform::JoinPath(root, "Resources"));
    MonoScript* script = RegisterScript(scripting::TypeOf<PostProcessData>(), "d0000000000000000000000000000010");
    auto* data = static_cast<PostProcessData*>(ScriptableObject::CreateInstance(scripting::TypeOf<PostProcessData>()));
    HX_REQUIRE(data != nullptr);
    data->SetName("PostData");
    Texture2D* grain = Texture2D::Create(2, 2, TextureFormat::RGBA32, false, true);
    Texture2D* area = Texture2D::Create(4, 4, TextureFormat::RGBA32, false, true);
    grain->SetName("Grain"), area->SetName("Area");
    data->SetTextureListCount(1, 1);
    data->SetTextureListAt(1, 0, grain);
    data->SetTextureAt(0, area);
    Array<u8> bytes;
    String error;
    Object* objects[] = {data, grain, area};
    HX_REQUIRE(WriteAssetFile(Span<Object* const>(objects, 3), bytes, error));
    StringView text(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    bool named = text.Contains("  shaders:\n    stopNanPS: {fileID: 0}\n") && text.Contains("    uberPostPS: {fileID: 0}\n") &&
                 text.Contains("  textures:\n    blueNoise16LTex: []\n    filmGrainTex:\n    - {fileID: ") && text.Contains("    smaaSearchTex: {fileID: 0}\n");
    if (!named)
        std::printf("%.*s\n", int(text.Size()), text.Data());
    HX_CHECK(named);
    platform::WriteFile(platform::JoinPath(root, "Resources/PostData.asset"), Span<const u8>(bytes.Data(), bytes.Size()));
    DestroyImmediate(data);
    DestroyImmediate(grain);
    DestroyImmediate(area);
    auto* source = new FolderAssetSource(root);
    AddAssetSource(source);
    auto* read = static_cast<PostProcessData*>(Resources::Load("PostData"));
    HX_REQUIRE(read != nullptr);
    HX_CHECK(read->TextureListCount(1) == 1 && read->TextureListAt(1, 0) && read->TextureListAt(1, 0)->width() == 2 && read->TextureAt(0) &&
             read->TextureAt(0)->width() == 4 && read->TextureAt(1) == nullptr && read->ShaderAt(8) == nullptr);
    ResetAssets();
    RemoveAssetSource(source);
    delete source;
    DestroyImmediate(script, true);
    platform::RemoveFile(platform::JoinPath(root, "Resources/PostData.asset"));
    platform::RemoveEmptyDirectory(platform::JoinPath(root, "Resources"));
    platform::RemoveEmptyDirectory(root);
    engine::testing::Cleanup();
}
