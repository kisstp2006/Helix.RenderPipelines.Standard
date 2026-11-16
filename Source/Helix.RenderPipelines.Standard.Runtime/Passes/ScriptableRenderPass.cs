using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using HelixEngine.Rendering.RenderGraphModule;

namespace HelixEngine.Rendering.Universal
{
    // The C# side of a pass: a C# class below this one records in C# (the native renderer calls its RecordRenderGraph
    // through the functions set here).
    public partial class ScriptableRenderPass
    {
        ProfilingSampler m_Sampler;

        static unsafe ScriptableRenderPass() =>
            SetManaged((ulong)(delegate* unmanaged[Cdecl]<ulong, ulong, ulong, byte>)&RecordManaged, (ulong)(delegate* unmanaged[Cdecl]<ulong, ulong, byte>)&CleanupManaged);

        /// <summary>The pass's passes on the frame's render graph (the frame data holds the camera's, the lights', the
        /// resources').</summary>
        public virtual void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData) { }

        /// <summary>The camera's frame ended: what the pass set for it goes.</summary>
        public virtual void OnCameraCleanup(CommandBuffer cmd) { }

        /// <summary>The sampler around the pass's commands (named as the pass's class).</summary>
        public ProfilingSampler profilingSampler
        {
            get => m_Sampler ??= new ProfilingSampler(string.IsNullOrEmpty(passName) ? GetType().Name : passName);
            set => m_Sampler = value;
        }

        public static bool operator <(ScriptableRenderPass lhs, ScriptableRenderPass rhs) => lhs.renderPassEvent < rhs.renderPassEvent;
        public static bool operator >(ScriptableRenderPass lhs, ScriptableRenderPass rhs) => lhs.renderPassEvent > rhs.renderPassEvent;

        /// <summary>Drawing settings of a pass of a light mode for the frame: the camera's sorting, the pipeline's per
        /// object data, the main light.</summary>
        public static DrawingSettings CreateDrawingSettings(ShaderTagId shaderTagId, UniversalRenderingData renderingData, UniversalCameraData cameraData,
                                                           UniversalLightData lightData, SortingCriteria sortingCriteria)
        {
            var sorting = new SortingSettings(cameraData.camera) { criteria = sortingCriteria };
            return new DrawingSettings(shaderTagId, sorting)
            {
                perObjectData = renderingData.perObjectData,
                mainLightIndex = lightData.mainLightIndex,
                enableDynamicBatching = renderingData.supportsDynamicBatching,
            };
        }

        public static DrawingSettings CreateDrawingSettings(List<ShaderTagId> shaderTagIdList, UniversalRenderingData renderingData, UniversalCameraData cameraData,
                                                           UniversalLightData lightData, SortingCriteria sortingCriteria)
        {
            if (shaderTagIdList == null || shaderTagIdList.Count == 0)
                return CreateDrawingSettings(ShaderTagId.none, renderingData, cameraData, lightData, sortingCriteria);
            DrawingSettings settings = CreateDrawingSettings(shaderTagIdList[0], renderingData, cameraData, lightData, sortingCriteria);
            for (int i = 1; i < shaderTagIdList.Count; i++)
                settings.SetShaderPassName(i, shaderTagIdList[i]);
            return settings;
        }

        static bool IsCSharp(ScriptableRenderPass pass) => ((global::Helix.Interop.INativeObject)pass).HxDerived;

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static byte RecordManaged(ulong wrapper, ulong graph, ulong frame)
        {
            try
            {
                if (GCHandle.FromIntPtr((IntPtr)wrapper).Target is not ScriptableRenderPass pass || !IsCSharp(pass))
                    return 0;
                pass.RecordRenderGraph(global::Helix.Interop.RenderGraphInterop.GraphOf(graph), global::Helix.Interop.RenderGraphInterop.ContainerOf(frame));
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
            return 1;
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static byte CleanupManaged(ulong wrapper, ulong cmd)
        {
            try
            {
                if (GCHandle.FromIntPtr((IntPtr)wrapper).Target is not ScriptableRenderPass pass || !IsCSharp(pass))
                    return 0;
                pass.OnCameraCleanup(global::Helix.Interop.RenderingInterop.Borrow(cmd));
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
            return 1;
        }
    }
}
