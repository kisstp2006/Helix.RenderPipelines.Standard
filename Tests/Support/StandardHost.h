#pragma once

#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Rendering/Pipeline/RenderPipeline.h>
#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Platform/Window/Window.h>
#include <Helix/RHI/Device/Device.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Scripting/Host/DotnetHost.h>
#include <Helix/Scripting/Types/Type.h>

#include <Support/TestClasses.h>

#include <CoreShaderLibraryAssembly.h>
#include <CoreShadersAssembly.h>
#include <StandardShadersAssembly.h>
#include <StandardTestShadersAssembly.h>
#include <TestShadersAssembly.h>

#include <bit>
#include <cstdio>
#include <cstdlib>

// The publish functions of the core's and the pipeline's modules (in their libraries on the desktop, linked in on the Web).
extern "C" void HxBindings_HelixRenderPipelinesCore_Publish();
extern "C" void HxBindings_HelixRenderPipelinesStandard_Publish();
// The test program's own game module (its C++ features).
HX_BINDINGS_EXPORT void HxBindings_RenderPipelinesStandardTests_Publish();

// The engine's C# side with the core's and the pipeline's runtimes and the C# tests, started once per process; the
// notes the tests write; the devices the pipeline runs on; the shaders the tests draw with as assets.
namespace helix::rpstandard::testing
{
    inline Array<String>& Notes()
    {
        static Array<String> notes;
        return notes;
    }

    inline void NoteFromCSharp(const char* text, i32 size) { Notes().Push(String(StringView(text, usize(size)))); }

    inline bool StartScripts()
    {
        static int state = 0;
        if (state != 0)
            return state > 0;
        state = -1;
        using namespace scripting;
        RegisterNativeExport("StandardTests.Note", std::bit_cast<const void*>(&NoteFromCSharp));
        String error;
        HxBindings_HelixRenderPipelinesCore_Publish();
        HxBindings_HelixRenderPipelinesStandard_Publish();
        HxBindings_RenderPipelinesStandardTests_Publish();
        for (const char* module : {"HelixRenderPipelinesCore", "HelixRenderPipelinesStandard", "RenderPipelinesStandardTests"})
            if (!engine::RegisterLinkedGameModule(module, error))
            {
                std::printf("    %.*s\n", int(error.Size()), error.Data());
                return false;
            }
        DotnetHostDesc desc;
        desc.runtimeAssembly = HX_TEST_RUNTIME_ASSEMBLY;
        if (!DotnetHost::Start(desc, error))
        {
            std::printf("    %.*s\n", int(error.Size()), error.Data());
            return false;
        }
        (void)FindType("HelixEngine.Object");
        AssemblyHandle core = DotnetHost::LoadAssembly(HX_TEST_CORE_MODULE, error);
        auto start = std::bit_cast<i32 (*)()>(DotnetHost::GetFunction(core, "HelixEngine.Internal.ScriptBridge", "Start", error));
        auto load = std::bit_cast<i32 (*)(const char*, i32)>(DotnetHost::GetFunction(core, "HelixEngine.Internal.ScriptBridge", "LoadScripts", error));
        if (!start || !load || start() != 0)
        {
            std::printf("    the engine's C# side did not start: %.*s\n", int(error.Size()), error.Data());
            return false;
        }
        // The core's and the pipeline's runtimes (their classes are the native modules', so they register none of their
        // own), then the tests.
        for (StringView runtime : {StringView(HX_TEST_CORE_RUNTIME), StringView(HX_TEST_STANDARD_RUNTIME)})
            if (load(runtime.Data(), i32(runtime.Size())) < 0)
            {
                std::printf("    %.*s did not load\n", int(runtime.Size()), runtime.Data());
                return false;
            }
        StringView scripts = HX_TEST_SCRIPTS;
        if (load(scripts.Data(), i32(scripts.Size())) <= 0)
        {
            std::printf("    no C# test scripts were registered\n");
            return false;
        }
        state = 1;
        return true;
    }

    inline const engine::ScriptClass& CSharp(const char* name)
    {
        const engine::ScriptClass* s = engine::FindScriptClass("Helix.RenderPipelines.Standard.Tests", "Tests", name);
        HX_ASSERT(s != nullptr);
        return *s;
    }

    // A device the pipeline runs on, as the tests name it.
    struct DeviceKind
    {
        rhi::Backend backend;
        // OpenGL and OpenGL ES: the newest version the context may have (0: the driver's newest).
        u32 maxGLVersion = 0;
        const char* name = "";
    };

    // The pipeline's devices: each D3D11 adapter, the newest OpenGL, OpenGL 3.3 (the oldest desktop GL it supports),
    // OpenGL ES, WebGL 2 where there is a window. f(device, label) runs on each that can be made; their number.
    template <class F>
    u32 ForEachDevice(F&& f)
    {
        static constexpr DeviceKind kKinds[] = {{rhi::Backend::D3D11, 0, "D3D11"},     {rhi::Backend::OpenGL, 0, "OpenGL"},
                                                {rhi::Backend::OpenGL, 33, "OpenGL 3.3"}, {rhi::Backend::OpenGLES, 0, "OpenGL ES"},
                                                {rhi::Backend::WebGL2, 0, "WebGL 2"}};
        u32 count = 0;
        for (const DeviceKind& kind : kKinds)
        {
            rhi::Backend b = kind.backend;
            if ((b == rhi::Backend::WebGL2 && !platform::Window::IsSupported()) || !rhi::Device::IsBackendAvailable(b))
                continue;
            char name[128];
            u32 adapters = 0;
            while (b == rhi::Backend::D3D11 && rhi::Device::AdapterName(b, adapters, name, sizeof(name)))
                adapters++;
            for (i32 adapter = adapters ? 0 : -1; adapter < i32(adapters); adapter++)
            {
                // HELIX_RENDER_BATCHING=1: the persistent material and object blocks; HELIX_RENDER_THREAD=1: a render thread owns the backend.
                static const bool batching = std::getenv("HELIX_RENDER_BATCHING") != nullptr;
                static const bool threaded = std::getenv("HELIX_RENDER_THREAD") != nullptr;
                engine::GraphicsSettings::SetUseScriptableRenderPipelineBatching(batching);
                rhi::Device* device =
                    rhi::Device::Create({.backend = b, .adapter = adapter, .maxGLVersion = kind.maxGLVersion, .vsync = false, .threaded = threaded});
                if (!device)
                    continue;
                String label = Format("{} ({}.{}) {}", kind.name, device->Caps().apiMajor, device->Caps().apiMinor, StringView(device->Caps().renderer));
                std::printf("    %s\n", label.CStr());
                f(*device, StringView(label));
                engine::Graphics::SetDevice(nullptr);
                rhi::Device::Destroy(device);
                count++;
            }
        }
        return count;
    }

    // The notes so far against the expected lines (all printed when they differ); the notes are cleared.
    inline bool SameNotes(std::initializer_list<const char*> expected)
    {
        Array<String> lines = Move(Notes());
        Notes() = Array<String>();
        bool same = lines.Size() == expected.size();
        usize i = 0;
        for (const char* e : expected)
            same = same && i < lines.Size() && lines[i++] == StringView(e);
        if (!same)
            for (const String& l : lines)
                std::printf("    %s\n", l.CStr());
        return same;
    }

    // The engine's test shaders, the core's shaders with their shader library and the pipeline's shaders as assets of a
    // temporary folder.
    struct ShaderFolder
    {
        String root;
        engine::FolderAssetSource* source = nullptr;
        static constexpr const char* kNames[] = {"HelixEngine.TestShaders.dll", "Helix.RenderPipelines.Core.ShaderLibrary.dll",
                                                 "Helix.RenderPipelines.Core.Shaders.dll", "Helix.RenderPipelines.Standard.Shaders.dll",
                                                 "Helix.RenderPipelines.Standard.TestShaders.dll"};

        ShaderFolder()
        {
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-standard-shaders-{}", Guid::Generate().ToString()));
            platform::CreateDirectories(root);
            Write(kNames[0], Span<const u8>(kTestShadersAssembly, sizeof(kTestShadersAssembly)));
            Write(kNames[1], Span<const u8>(kCoreShaderLibraryAssembly, sizeof(kCoreShaderLibraryAssembly)));
            Write(kNames[2], Span<const u8>(kCoreShadersAssembly, sizeof(kCoreShadersAssembly)));
            Write(kNames[3], Span<const u8>(kStandardShadersAssembly, sizeof(kStandardShadersAssembly)));
            Write(kNames[4], Span<const u8>(kStandardTestShadersAssembly, sizeof(kStandardTestShadersAssembly)));
            source = new engine::FolderAssetSource(root);
            engine::AddAssetSource(source);
            for (const char* name : kNames)
                if (const engine::AssetEntry* e = engine::FindAssetAtPath(name))
                    engine::LoadAsset(e->guid);
        }

        ~ShaderFolder()
        {
            engine::ResetAssets();
            engine::testing::Cleanup();
            engine::Shader::Globals().Clear();
            delete source;
            for (const char* name : kNames)
                platform::RemoveFile(platform::JoinPath(root, name));
            platform::RemoveEmptyDirectory(root);
        }

        void Write(const char* name, Span<const u8> bytes) { platform::WriteFile(platform::JoinPath(root, name), bytes); }
    };
}
