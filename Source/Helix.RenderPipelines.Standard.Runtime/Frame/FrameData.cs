using System.Collections.Generic;
using Helix.Collections;
using Helix.Collections.LowLevel.Unsafe;
using HelixEngine.Rendering.RenderGraphModule;

namespace HelixEngine.Rendering.Universal
{
    // The members of the frame data whose values are the core's or the engine's C# types.
    public partial class UniversalResourceData
    {
        TextureHandle At(int which) => global::Helix.Interop.RenderGraphInterop.TextureOf(Packed(which));
        void Set(int which, TextureHandle value) => SetPacked(which, global::Helix.Interop.RenderGraphInterop.PackedOf(value));

        public TextureHandle backBufferColor { get => At(0); internal set => Set(0, value); }
        public TextureHandle backBufferDepth { get => At(1); internal set => Set(1, value); }
        public TextureHandle cameraColor { get => At(2); set => Set(2, value); }
        public TextureHandle cameraDepth { get => At(3); set => Set(3, value); }
        public TextureHandle cameraDepthTexture { get => At(4); set => Set(4, value); }
        public TextureHandle cameraNormalsTexture { get => At(5); set => Set(5, value); }
        public TextureHandle cameraOpaqueTexture { get => At(6); set => Set(6, value); }
        public TextureHandle afterPostProcessColor { get => At(7); set => Set(7, value); }
        public TextureHandle internalColorLut { get => At(8); set => Set(8, value); }
        public TextureHandle mainShadowsTexture { get => At(9); set => Set(9, value); }
        /// <summary>The color the passes draw into now: the back buffer's or the camera's.</summary>
        public TextureHandle activeColorTexture => At(10);
        public TextureHandle activeDepthTexture => At(11);
    }

    public partial class UniversalCameraData
    {
        /// <summary>The camera's target as a descriptor (its size at the render scale, format, samples).</summary>
        public RenderTextureDescriptor cameraTargetDescriptor
        {
            get => global::Helix.Interop.RenderingInterop.DescriptorAt(DescriptorAddress());
            set
            {
                global::Helix.Interop.RenderingInterop.WriteDescriptorAt(DescriptorAddress(), value);
                ApplyDescriptor();
            }
        }

        public LayerMask volumeLayerMask
        {
            get => volumeLayerMaskBits;
            set => volumeLayerMaskBits = value;
        }
    }

    public partial class UniversalLightData
    {
        /// <summary>The visible lights of the frame (the culling's memory, valid during the frame).</summary>
        public unsafe NativeArray<VisibleLight> visibleLights =>
            NativeArrayUnsafeUtility.ConvertExistingDataToNativeArray<VisibleLight>((void*)VisibleLightsAddress(), VisibleLightCount(), Allocator.None);
    }

    public partial class UniversalShadowData
    {
        /// <summary>Per visible light its depth bias (x) and normal bias (y).</summary>
        public List<Vector4> bias => new List<Vector4>(biasArray);
    }

    public partial class UniversalRenderingData
    {
        public CullingResults cullResults => global::Helix.Interop.RenderingInterop.CullingResultsOf(CullResultsId());

        public LayerMask opaqueLayerMask
        {
            get => opaqueLayerMaskBits;
            set => opaqueLayerMaskBits = value;
        }

        public LayerMask transparentLayerMask
        {
            get => transparentLayerMaskBits;
            set => transparentLayerMaskBits = value;
        }
    }
}
