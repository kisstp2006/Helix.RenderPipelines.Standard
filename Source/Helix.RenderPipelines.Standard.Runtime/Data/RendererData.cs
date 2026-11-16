using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace HelixEngine.Rendering.Universal
{
    // The feature list of renderer data as the reference has it: a List; changes to it reach the data at SetDirty. A
    // C# class below it makes its renderer in Create (the pipeline asset calls it through the function set here).
    public partial class ScriptableRendererData
    {
        static unsafe ScriptableRendererData() => SetManaged((ulong)(delegate* unmanaged[Cdecl]<ulong, ulong*, ulong>)&CreateManaged);

        /// <summary>A new renderer of this data (the pipeline asset keeps it); C# renderer data makes its own.</summary>
        protected virtual ScriptableRenderer Create() => null;

        /// <summary>Enabled: its renderers are made again.</summary>
        protected virtual void OnEnable() => SetDirty();

        /// <summary>Its values changed: its renderers are made again.</summary>
        protected virtual void OnValidate() => SetDirty();

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static unsafe ulong CreateManaged(ulong wrapper, ulong* handle)
        {
            *handle = 0;
            try
            {
                if (GCHandle.FromIntPtr((IntPtr)wrapper).Target is not ScriptableRendererData data)
                    return 0;
                ScriptableRenderer renderer = data.Create();
                if (renderer == null)
                    return 0;
                // The asset keeps the renderer's C# object alive with this handle.
                *handle = (ulong)GCHandle.ToIntPtr(GCHandle.Alloc(renderer));
                return ((global::Helix.Interop.INativeObject)renderer).HxId;
            }
            catch (Exception e)
            {
                Debug.LogException(e);
                return 0;
            }
        }

        List<ScriptableRendererFeature> m_Features;
        // The data's features when the list was made (it is made again when they changed on the native side).
        ScriptableRendererFeature[] m_Taken;

        public List<ScriptableRendererFeature> rendererFeatures
        {
            get
            {
                ScriptableRendererFeature[] now = rendererFeaturesArray;
                if (m_Features == null || !Same(m_Taken, now))
                {
                    m_Features = new List<ScriptableRendererFeature>(now);
                    m_Taken = now;
                }
                return m_Features;
            }
        }

        /// <summary>The renderers of this data are made again: changes to rendererFeatures are taken.</summary>
        public void SetDirty()
        {
            if (m_Features != null)
            {
                m_Taken = m_Features.ToArray();
                rendererFeaturesArray = m_Taken;
            }
            SetDirtyNative();
        }

        /// <summary>The first feature of the type (active or not).</summary>
        public bool TryGetRendererFeature<T>(out T rendererFeature) where T : ScriptableRendererFeature
        {
            foreach (ScriptableRendererFeature f in rendererFeaturesArray)
                if (f is T t)
                {
                    rendererFeature = t;
                    return true;
                }
            rendererFeature = null;
            return false;
        }

        static bool Same(ScriptableRendererFeature[] a, ScriptableRendererFeature[] b)
        {
            if (a == null || a.Length != b.Length)
                return false;
            for (int i = 0; i < b.Length; i++)
                if (a[i] != b[i])
                    return false;
            return true;
        }
    }

    public partial class UniversalRendererData
    {
        /// <summary>The forward renderer of this data (C# classes below this one).</summary>
        protected override ScriptableRenderer Create() => new UniversalRenderer(this);

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
