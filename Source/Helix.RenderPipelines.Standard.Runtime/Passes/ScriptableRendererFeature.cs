using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace HelixEngine.Rendering.Universal
{
    // The C# side of a feature: a C# feature's Create, AddRenderPasses and OnCameraPreCull run in C# (the native
    // renderer calls them through the function set here). They are virtual (abstract in the reference): the C# classes
    // of C++ features are made from the native module's bindings.
    public partial class ScriptableRendererFeature : IDisposable
    {
        static unsafe ScriptableRendererFeature() => SetManaged((ulong)(delegate* unmanaged[Cdecl]<ulong, int, ulong, ulong, void>)&CallManaged);

        /// <summary>The feature's passes are made (when it is enabled).</summary>
        public virtual void Create() { }

        /// <summary>Its passes for a camera's frame, queued into the renderer (EnqueuePass).</summary>
        public virtual void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData) { }

        /// <summary>Before the camera is culled.</summary>
        public virtual void OnCameraPreCull(ScriptableRenderer renderer, in CameraData cameraData) { }

        protected virtual void OnEnable() => Create();

        protected virtual void Dispose(bool disposing) { }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static void CallManaged(ulong wrapper, int call, ulong renderer, ulong frame)
        {
            try
            {
                if (GCHandle.FromIntPtr((IntPtr)wrapper).Target is not ScriptableRendererFeature feature)
                    return;
                ScriptableRenderer r = ScriptableRendererNative.Of(renderer);
                ContextContainer frameData = global::Helix.Interop.RenderGraphInterop.ContainerOf(frame);
                switch (call)
                {
                case 0:
                    feature.Create();
                    break;
                case 1:
                    var renderingData = new RenderingData(frameData);
                    feature.AddRenderPasses(r, ref renderingData);
                    break;
                case 2:
                    feature.OnCameraPreCull(r, new CameraData(frameData));
                    break;
                case 3:
                    feature.Dispose(true);
                    break;
                }
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }
    }
}
