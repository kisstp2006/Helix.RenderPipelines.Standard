#include <Support/StandardHost.h>

#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/RenderPipelines/Standard/Frame/FrameData.h>
#include <Helix/Testing/Test.h>

#include <bit>

using namespace helix;
using namespace helix::engine;
using namespace helix::rendering;
using namespace helix::rendering::universal;
using namespace helix::rpstandard::testing;

namespace
{
    ContextContainer* gFrame = nullptr;
    u64 FrameForCSharp() { return gFrame ? gFrame->Id() : 0; }
}

HX_TEST(Standard, FrameDataInBothLanguages)
{
    // C++ fills a frame's camera and light data; C# reads them through the same container and changes them; a
    // container of C# makes the pipeline's items; Dispose resets them.
    HX_REQUIRE(StartScripts());
    scripting::RegisterNativeExport("StandardTests.Frame", std::bit_cast<const void*>(&FrameForCSharp));
    ContextContainer frame;
    gFrame = &frame;
    UniversalCameraData& camera = frame.Create<UniversalCameraData>();
    camera.scaledWidth = 320;
    camera.renderScale = 0.5f;
    camera.cameraTargetDescriptor = RenderTextureDescriptor(320, 180);
    frame.GetOrCreate<UniversalLightData>().mainLightIndex = 2;
    HX_CHECK(frame.Contains<UniversalCameraData>() && !frame.Contains<UniversalShadowData>());
    Notes().Clear();
    GameObject* host = GameObject::Create("FrameHost");
    host->AddComponent(CSharp("CsFrameTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(SameNotes({"native:320:0.5:True:320x180:2:False:True", "csharp:500000003:True:2:6:0", "reset:False:0:-1"}));
    HX_CHECK(camera.scaledHeight == 99 && camera.cameraTargetDescriptor.width == 640);
    frame.Dispose();
    HX_CHECK(camera.scaledWidth == 0 && !frame.Contains<UniversalCameraData>() && &frame.GetOrCreate<UniversalCameraData>() == &camera);
    gFrame = nullptr;
    DestroyImmediate(host);
}
