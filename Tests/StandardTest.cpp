#include <Support/StandardHost.h>

#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Rendering/Renderer.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/RenderPipelines/Standard/Settings/PipelineSettings.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::engine;
using namespace helix::rpstandard::testing;

namespace
{
    bool NearColor(u32 a, u32 b, i32 tolerance = 3)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            i32 ca = i32((a >> shift) & 0xFF), cb = i32((b >> shift) & 0xFF);
            if (ca - cb > tolerance || cb - ca > tolerance)
                return false;
        }
        return true;
    }

    constexpr u32 Rgba(u32 r, u32 g, u32 b, u32 a = 255) { return r | g << 8 | b << 16 | a << 24; }
}

HX_TEST(Standard, SettingsAreTheSameInBothLanguages)
{
    // The native module's enums, as C# reads them.
    using namespace helix::rendering::universal;
    HX_REQUIRE(StartScripts());
    static_assert(i32(ShadowResolution::_2048) == 2048 && i32(RenderPassEvent::AfterRenderingOpaques) == 300);
    Notes().Clear();
    GameObject* host = GameObject::Create("SettingsHost");
    host->AddComponent(CSharp("CsSettingsTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(SameNotes({"shadows:2:2048:3", "msaa:4:2:2:1", "events:300:550:AfterRendering",
                        "inputs:Depth, Color:True:Overlay:UsePipelineSettings:Forward:Store"}));
    DestroyImmediate(host);
}

HX_TEST(Standard, TheFallbackErrorShaderDrawsOnEveryDevice)
{
    // The pipeline's error shader on every device the pipeline runs on: magenta where the quad is, the clear color
    // around it; OpenGL 3.3 gets a 3.3 context.
    HX_REQUIRE(StartScripts());
    constexpr u32 size = 16;
    bool gl33 = false;
    u32 devices = ForEachDevice([&](rhi::Device& device, StringView label) {
        ShaderFolder shaders;
        if (label.StartsWith("OpenGL 3.3"))
        {
            gl33 = true;
            HX_CHECK(device.Caps().apiMajor == 3 && device.Caps().apiMinor == 3);
        }
        rhi::TextureHandle color =
            device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = size, .height = size, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
        rhi::TextureHandle depth = device.CreateTexture({.format = rhi::Format::D32Float, .width = size, .height = size, .usage = rhi::TextureDepthStencil});
        Graphics::SetDevice(&device);
        Graphics::SetTarget({color, depth, size, size, rhi::Format::RGBA8Unorm, rhi::Format::D32Float});
        Shader* error = Shader::Find("Hidden/Universal Render Pipeline/FallbackError");
        HX_REQUIRE(error != nullptr);
        GameObject* eye = GameObject::Create("Eye");
        Camera* camera = eye->AddComponent<Camera>();
        eye->transform()->SetPosition(Vector3(0, 0, -10));
        camera->SetOrthographic(true);
        camera->SetOrthographicSize(1);
        camera->SetClearFlags(CameraClearFlags::SolidColor);
        camera->SetBackgroundColor(Color(0, 0, 0, 1));
        Mesh* quad = Mesh::Create();
        const Vector3 vertices[] = {{-0.5f, -0.5f, 0}, {-0.5f, 0.5f, 0}, {0.5f, 0.5f, 0}, {0.5f, -0.5f, 0}};
        const i32 triangles[] = {0, 1, 2, 0, 2, 3};
        quad->SetVertices(Span<const Vector3>(vertices, 4));
        quad->SetTriangles(Span<const i32>(triangles, 6));
        quad->RecalculateBounds();
        Material* material = Material::Create(error);
        GameObject* thing = GameObject::Create("Errored");
        thing->AddComponent<MeshFilter>()->SetSharedMesh(quad);
        thing->AddComponent<MeshRenderer>()->SetSharedMaterial(material);
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(size * size);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        u32 middle = pixels[8 * size + 8], corner = pixels[1 * size + 1];
        bool ok = NearColor(middle, Rgba(255, 0, 255)) && NearColor(corner, Rgba(0, 0, 0));
        if (!ok)
            std::printf("    %.*s: middle %08x corner %08x\n", int(label.Size()), label.Data(), middle, corner);
        HX_CHECK(ok);
        DestroyImmediate(thing);
        DestroyImmediate(eye);
        DestroyImmediate(material);
        DestroyImmediate(quad);
        Graphics::ReleaseResources();
        device.Destroy(color);
        device.Destroy(depth);
    });
    // In node on the Web there is no window, so no device.
    bool any = rhi::Device::IsBackendAvailable(rhi::Backend::D3D11) || rhi::Device::IsBackendAvailable(rhi::Backend::OpenGL) ||
               (platform::Window::IsSupported() && rhi::Device::IsBackendAvailable(rhi::Backend::WebGL2));
    std::printf("    %u devices\n", devices);
    HX_CHECK(devices > 0 || !any);
    if (rhi::Device::IsBackendAvailable(rhi::Backend::OpenGL))
        HX_CHECK(gl33);
}
