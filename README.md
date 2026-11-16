# Helix.RenderPipelines.Standard

The Standard render pipeline of the Helix engine: a forward pipeline for every device the engine runs on, from
OpenGL 3.3, OpenGL ES and WebGL 2 to D3D11. It is written in C++ over the engine and the shared render pipeline core
(Helix.RenderPipelines.Core), with C# bindings that give C# the familiar C# API (namespace
HelixEngine.Rendering.Universal). Its shaders are C#, compiled from their IL by the engine when they load, and call
the core's shader library.

The pipeline is a native module (`HelixRenderPipelinesStandard`, a shared library beside the engine's and the core's;
a static one on the Web). Its C# side (`Helix.RenderPipelines.Standard.Runtime`) holds the generated bindings and
the C# written by hand over them. Renderers, renderer features and passes can be written in either language and mixed
on one renderer.

## What it draws

- Forward rendering with per object light lists: the main light and additional point and spot lights.
- The main light's cascaded shadows (1 to 4 cascades in one atlas), depth and normal bias, soft shadows in three
  quality tiers, the fade at the shadow distance.
- The surface shaders `Universal Render Pipeline/Lit` (metallic and specular workflows, normal, occlusion and emission
  maps, alpha clipping, transparency), `Simple Lit`, `Unlit` and `Particles/Unlit`, with the reference's properties,
  keywords and render states set from material properties.
- The ambient light, reflection probes and the scene's reflection; the skybox behind the opaques.
- The camera's depth and opaque textures (copies or a depth prepass), render scale, MSAA, HDR in the asset's precision,
  the back buffer path when nothing needs an intermediate texture.
- Post processing through volumes: Bloom, Tonemapping (Neutral, ACES) and ColorAdjustments, graded with a color
  grading look up table in LDR or HDR.

## Contents

| Path | Content |
|---|---|
| `Source/Helix/RenderPipelines/Standard/Settings/` | The pipeline's enums: shadow and MSAA settings, rendering modes, render pass events and inputs |
| `Source/Helix/RenderPipelines/Standard/Pipeline/` | `UniversalRenderPipelineAsset` (the asset's settings, renderers, default materials, version upgrades) and `UniversalRenderPipeline` (the frame data of each camera, the renderers' frames) |
| `Source/Helix/RenderPipelines/Standard/Renderer/` | `ScriptableRenderer`, `UniversalRenderer` (the forward frame on the core's render graph), renderer data (`UniversalRendererData`), renderer features, `PostProcessData` |
| `Source/Helix/RenderPipelines/Standard/Passes/` | `ScriptableRenderPass`, the post processing passes (the grading table, bloom, the uber pass) |
| `Source/Helix/RenderPipelines/Standard/Frame/` | The frame data items (`UniversalCameraData`, `UniversalLightData`, `UniversalShadowData`, `UniversalResourceData`, ...) |
| `Source/Helix/RenderPipelines/Standard/Shadows/` | `ShadowUtils` (over the core's `CascadedShadows`, which records the cascades for this pipeline and the Physical one) |
| `Source/Helix/RenderPipelines/Standard/Overrides/` | The volume components `Bloom`, `Tonemapping`, `ColorAdjustments` |
| `Source/Helix/RenderPipelines/Standard/Components/` | `UniversalAdditionalCameraData`, `UniversalAdditionalLightData` |
| `Source/Helix.RenderPipelines.Standard.Runtime/` | The C# side: the generated bindings and the C# API written over them (global settings, renderer data and renderers of C# classes, the C# helpers) |
| `Shaders/` | The pipeline's C# shaders and their library (`Shaders/Library/`): Lit, Simple Lit, Unlit, Particles/Unlit, the copy shaders, Bloom, the look up table builders, UberPost, FallbackError |
| `Tests/` | Tests in both languages run in the engine by a C++ test program on every device the pipeline supports (each D3D11 adapter, the newest OpenGL, an OpenGL 3.3 context, OpenGL ES; WebGL 2 on the Web), with golden images in `Tests/Golden/`; `Tests/Api/Helix.RenderPipelines.Standard.csv` is the inventory of the public C# surface |

## Use

In C++: an asset of the pipeline, its settings, a volume with post processing.

```cpp
#include <Helix/RenderPipelines/Core/Volume/Volume.h>
#include <Helix/RenderPipelines/Standard/Overrides/Overrides.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipelineAsset.h>

using namespace helix;
using namespace helix::rendering;
using namespace helix::rendering::universal;

UniversalRenderPipelineAsset* asset = UniversalRenderPipelineAsset::Create();
asset->SetShadowCascadeCount(2);
asset->SetSupportsSoftShadows(true);
engine::GraphicsSettings::SetDefaultRenderPipeline(asset);

auto* profile = static_cast<VolumeProfile*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<VolumeProfile>()));
profile->Add<Bloom>()->intensity.Override(0.5f);
profile->Add<Tonemapping>()->mode.Override(TonemappingMode::ACES);
```

In C#: a renderer feature with a pass on the render graph.

```csharp
using HelixEngine.Rendering;
using HelixEngine.Rendering.RenderGraphModule;
using HelixEngine.Rendering.Universal;

public class OutlineFeature : ScriptableRendererFeature
{
    OutlinePass m_Pass;

    public override void Create() => m_Pass = new OutlinePass { renderPassEvent = RenderPassEvent.AfterRenderingOpaques };

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData) => renderer.EnqueuePass(m_Pass);
}

public class OutlinePass : ScriptableRenderPass
{
    public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
    {
        UniversalResourceData resources = frameData.Get<UniversalResourceData>();
        // ... AddRasterRenderPass into resources.activeColorTexture
    }
}
```

## Build

The repository builds with CMake presets (`windows-debug`, `windows-release`, `web-release`, `android-arm64-debug`).
Its dependencies are git submodules in `External/` (`git submodule update --init`, one level). The C# parts need the
.NET SDK (`HELIX_DOTNET_ROOT`).

```
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

## License

GPL-3.0, see `LICENSE`.
