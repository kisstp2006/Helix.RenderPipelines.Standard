#pragma once

#include <Support/Golden.h>
#include <Support/StandardHost.h>

#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Rendering/Light.h>
#include <Helix/Engine/Rendering/Renderer.h>
#include <Helix/Engine/Transform/Hierarchy.h>
#include <Helix/Engine/Transform/Transform.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipeline.h>

// The scene of the pipeline's frame tests and the device's target they draw into.
namespace helix::rpstandard::testing
{
    // A box of side 1 around the origin (faces clockwise seen from outside, with normals and uvs).
    inline engine::Mesh* TestBox()
    {
        Array<Vector3> v, n;
        Array<Vector2> uv;
        Array<i32> t;
        const Vector3 axes[3] = {Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)};
        for (i32 a = 0; a < 3; a++)
            for (f32 sign : {1.0f, -1.0f})
            {
                Vector3 normal = axes[a] * sign;
                Vector3 u = axes[(a + 1) % 3] * 0.5f, w = axes[(a + 2) % 3] * 0.5f;
                if (sign < 0)
                    std::swap(u, w);
                i32 base = i32(v.Size());
                Vector3 c = normal * 0.5f;
                for (Vector3 p : {c - u - w, c - u + w, c + u + w, c + u - w})
                    v.Push(p), n.Push(normal);
                for (Vector2 q : {Vector2(0, 0), Vector2(0, 1), Vector2(1, 1), Vector2(1, 0)})
                    uv.Push(q);
                for (i32 k : {0, 2, 1, 0, 3, 2})
                    t.Push(base + k);
            }
        engine::Mesh* m = engine::Mesh::Create();
        m->SetVertices(Span<const Vector3>(v.Data(), v.Size()));
        m->SetNormals(Span<const Vector3>(n.Data(), n.Size()));
        m->SetUVs(0, Span<const Vector2>(uv.Data(), uv.Size()));
        m->SetTriangles(Span<const i32>(t.Data(), t.Size()));
        m->RecalculateBounds();
        return m;
    }

    // A sphere of radius 0.5 around the origin (clockwise from outside), with normals, tangents, uvs and white vertex colors.
    inline engine::Mesh* TestSphere(i32 rings = 12, i32 segments = 24)
    {
        Array<Vector3> v, n;
        Array<Vector2> uv;
        Array<Color> colors;
        Array<i32> t;
        for (i32 r = 0; r <= rings; r++)
            for (i32 s = 0; s <= segments; s++)
            {
                f32 theta = mathf::PI * f32(r) / f32(rings), phi = 2.0f * mathf::PI * f32(s) / f32(segments);
                Vector3 d(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
                v.Push(d * 0.5f), n.Push(d), uv.Push(Vector2(f32(s) / f32(segments), 1.0f - f32(r) / f32(rings))), colors.Push(Color(1, 1, 1, 1));
            }
        for (i32 r = 0; r < rings; r++)
            for (i32 s = 0; s < segments; s++)
            {
                i32 a = r * (segments + 1) + s, b = a + segments + 1;
                for (i32 k : {a, a + 1, b, a + 1, b + 1, b})
                    t.Push(k);
            }
        engine::Mesh* m = engine::Mesh::Create();
        m->SetVertices(Span<const Vector3>(v.Data(), v.Size()));
        m->SetNormals(Span<const Vector3>(n.Data(), n.Size()));
        m->SetUVs(0, Span<const Vector2>(uv.Data(), uv.Size()));
        m->SetColors(Span<const Color>(colors.Data(), colors.Size()));
        m->SetTriangles(Span<const i32>(t.Data(), t.Size()));
        m->RecalculateTangents();
        m->RecalculateBounds();
        return m;
    }

    // A texture of a function of its texel's coordinates (0 to 1), linear or sRGB.
    template <class F>
    engine::Texture2D* TestTexture(i32 size, bool linear, F&& f, engine::FilterMode filter = engine::FilterMode::Bilinear)
    {
        engine::Texture2D* tex = engine::Texture2D::Create(size, size, engine::TextureFormat::RGBA32, false, linear);
        tex->SetFilterMode(filter);
        Array<Color> pixels;
        for (i32 y = 0; y < size; y++)
            for (i32 x = 0; x < size; x++)
                pixels.Push(f((f32(x) + 0.5f) / f32(size), (f32(y) + 0.5f) / f32(size)));
        tex->SetPixels(Span<const Color>(pixels.Data(), pixels.Size()));
        tex->Apply(false);
        return tex;
    }

    // A cube of one color.
    inline engine::Cubemap* TestCube(Color c)
    {
        engine::Cubemap* cube = engine::Cubemap::Create(1, engine::TextureFormat::RGBA32, false, true);
        for (i32 f = 0; f < 6; f++)
            cube->SetPixel(engine::CubemapFace(f), 0, 0, c);
        cube->Apply();
        return cube;
    }

    // A lit ground and box (the box's shadow on the ground from the sun), a point light's blue on the ground, an unlit
    // box and a transparent pane over the red box.
    struct TestScene
    {
        Array<engine::GameObject*> made;
        Array<engine::Object*> assets;
        engine::Mesh* box = nullptr;
        engine::Camera* camera = nullptr;

        engine::Material* Colored(const char* shader, Color color)
        {
            engine::Material* m = engine::Material::Create(engine::Shader::Find(shader));
            m->SetColor("_BaseColor", color);
            assets.Push(m);
            return m;
        }

        engine::GameObject* Thing(StringView name, engine::Material* material, Vector3 position, Quaternion rotation, Vector3 scale)
        {
            engine::GameObject* g = engine::GameObject::Create(name);
            g->transform()->SetPosition(position);
            g->transform()->SetRotation(rotation);
            g->transform()->SetLocalScale(scale);
            g->AddComponent<engine::MeshFilter>()->SetSharedMesh(box);
            g->AddComponent<engine::MeshRenderer>()->SetSharedMaterial(material);
            made.Push(g);
            return g;
        }

        TestScene()
        {
            using namespace engine;
            GameObject* eye = GameObject::Create("Eye");
            made.Push(eye);
            camera = eye->AddComponent<Camera>();
            eye->transform()->SetPosition(Vector3(0, 4, -9));
            eye->transform()->SetRotation(Quaternion::Euler(22, 0, 0));
            camera->SetClearFlags(CameraClearFlags::SolidColor);
            camera->SetBackgroundColor(Color(0, 0, 0.2f, 1));
            GameObject* sun = GameObject::Create("Sun");
            made.Push(sun);
            Light* sunLight = sun->AddComponent<Light>();
            sunLight->SetType(LightType::Directional);
            sunLight->SetShadows(LightShadows::Hard);
            sun->transform()->SetRotation(Quaternion::Euler(50, 150, 0));
            GameObject* lamp = GameObject::Create("Lamp");
            made.Push(lamp);
            Light* lampLight = lamp->AddComponent<Light>();
            lampLight->SetType(LightType::Point);
            lampLight->SetColor(Color(0.2f, 0.4f, 1, 1));
            lampLight->SetIntensity(3);
            lampLight->SetRange(3);
            lamp->transform()->SetPosition(Vector3(3, 0.5f, 0));

            box = TestBox();
            assets.Push(box);
            Thing("Ground", Colored("Tests/StandardLit", Color(0.9f, 0.9f, 0.9f, 1)), Vector3(0, -0.05f, 2), Quaternion::identity, Vector3(12, 0.1f, 10));
            Thing("Box", Colored("Tests/StandardLit", Color(1, 0.25f, 0.2f, 1)), Vector3(0, 0.75f, 0), Quaternion::Euler(0, 30, 0), Vector3(1.5f, 1.5f, 1.5f));
            Thing("Unlit", Colored("Tests/StandardUnlit", Color(0.2f, 0.9f, 0.3f, 1)), Vector3(-3, 0.5f, 0), Quaternion::identity, Vector3(1, 1, 1));
            Thing("Glass", Colored("Tests/StandardTransparent", Color(1, 1, 1, 0.4f)), Vector3(0.6f, 1.2f, -1.8f), Quaternion::identity, Vector3(1.2f, 1.2f, 0.1f));
        }

        ~TestScene()
        {
            // The pipeline goes before its asset.
            engine::GraphicsSettings::SetDefaultRenderPipeline(nullptr);
            engine::Graphics::ReleaseResources();
            for (engine::GameObject* g : made)
                engine::DestroyImmediate(g);
            for (engine::Object* o : assets)
                engine::DestroyImmediate(o);
        }
    };

    // The device's color and depth target of a frame test.
    struct TestTarget
    {
        rhi::Device& device;
        rhi::TextureHandle color, depth;
        u32 width, height;
        Array<u32> pixels;

        TestTarget(rhi::Device& d, u32 w, u32 h) : device(d), width(w), height(h)
        {
            color = device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = w, .height = h, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
            depth = device.CreateTexture({.format = rhi::Format::D32Float, .width = w, .height = h, .usage = rhi::TextureDepthStencil});
            engine::Graphics::SetDevice(&device);
            engine::Graphics::SetTarget({color, depth, w, h, rhi::Format::RGBA8Unorm, rhi::Format::D32Float});
            engine::Camera::SetDisplaySize(w, h);
            pixels.Resize(w * h);
        }

        // A frame rendered and read back.
        bool Render()
        {
            engine::UpdateWorldMatrices();
            engine::Graphics::RenderFrame();
            return device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4);
        }

        bool Matches(const char* golden, StringView label, u32 tolerance = 12)
        {
            bool same = engine::testing::MatchGolden(golden, pixels.Data(), width, height, tolerance);
            if (!same)
                std::printf("    %s differs on %.*s\n", golden, int(label.Size()), label.Data());
            return same;
        }

        ~TestTarget()
        {
            engine::GraphicsSettings::SetDefaultRenderPipeline(nullptr);
            engine::Camera::SetDisplaySize(1, 1);
            engine::Graphics::ReleaseResources();
            device.Destroy(color);
            device.Destroy(depth);
        }
    };
}
