using HelixEngine;
using HelixEngine.Rendering;
using HelixEngine.Rendering.RenderGraphModule;
using HelixEngine.Rendering.Universal;

namespace Tests
{
    // The frame data from C#: a container C++ filled (its camera data read and changed), and one of C# with the
    // pipeline's items made from C#.
    public class CsFrameTests : GameBehaviour
    {
        static unsafe ulong NativeFrame() => ((delegate* unmanaged[Cdecl]<ulong>)Helix.Scripting.Host.FindExport("StandardTests.Frame"))();

        void Start()
        {
            ContextContainer native = Helix.Interop.RenderGraphInterop.ContainerOf(NativeFrame());
            UniversalCameraData camera = native.Get<UniversalCameraData>();
            UniversalLightData lights = native.Get<UniversalLightData>();
            RenderTextureDescriptor d = camera.cameraTargetDescriptor;
            Notes.Note($"native:{camera.scaledWidth}:{camera.renderScale}:{camera.isGameCamera}:{d.width}x{d.height}:{lights.mainLightIndex}:" +
                       $"{native.Contains<UniversalShadowData>()}:{ReferenceEquals(camera, native.Get<UniversalCameraData>())}");
            camera.scaledHeight = 99;
            d.width = 640;
            camera.cameraTargetDescriptor = d;

            var frame = new ContextContainer();
            UniversalResourceData resources = frame.GetOrCreate<UniversalResourceData>();
            resources.cameraColor = Helix.Interop.RenderGraphInterop.TextureOf(0x0000000500000003ul);
            UniversalShadowData shadows = frame.Create<UniversalShadowData>();
            shadows.mainLightShadowCascadesCount = 2;
            UniversalRenderingData rendering = frame.GetOrCreate<UniversalRenderingData>();
            rendering.opaqueLayerMask = 6;
            Notes.Note($"csharp:{Helix.Interop.RenderGraphInterop.PackedOf(resources.cameraColor):X}:{resources.activeColorTexture.Equals(resources.cameraColor)}:" +
                       $"{shadows.mainLightShadowCascadesCount}:{rendering.opaqueLayerMask.value}:{shadows.bias.Count}");
            frame.Dispose();
            Notes.Note($"reset:{frame.Contains<UniversalShadowData>()}:{shadows.mainLightShadowCascadesCount}:{rendering.opaqueLayerMask.value}");
        }
    }
}
