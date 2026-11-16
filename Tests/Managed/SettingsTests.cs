using System;
using System.Text;
using Helix.Scripting;
using HelixEngine;
using HelixEngine.Rendering.Universal;

namespace Tests
{
    // The lines the C# tests write, read by the C++ test program (its export by name).
    static unsafe class Notes
    {
        static delegate* unmanaged[Cdecl]<byte*, int, void> s_Note;

        public static void Note(string message)
        {
            if (s_Note == null)
                s_Note = (delegate* unmanaged[Cdecl]<byte*, int, void>)Host.FindExport("StandardTests.Note");
            byte[] bytes = Encoding.UTF8.GetBytes(message);
            fixed (byte* p = bytes)
                s_Note(p, bytes.Length);
        }
    }

    // The pipeline's settings as C# sees them: the values of the native module's enums, the flags of the pass inputs.
    public class CsSettingsTests : GameBehaviour
    {
        void Start()
        {
            Notes.Note($"shadows:{(int)ShadowQuality.SoftShadows}:{(int)ShadowResolution._2048}:{(int)SoftShadowQuality.High}");
            Notes.Note($"msaa:{(int)MsaaQuality._4x}:{(int)Downsampling._4xBox}:{(int)LightRenderingMode.PerVertex}:{(int)ColorGradingMode.HighDynamicRange}");
            Notes.Note($"events:{(int)RenderPassEvent.AfterRenderingOpaques}:{(int)RenderPassEvent.BeforeRenderingPostProcessing}:{RenderPassEvent.AfterRendering}");
            ScriptableRenderPassInput input = ScriptableRenderPassInput.Depth | ScriptableRenderPassInput.Color;
            bool flags = Attribute.IsDefined(typeof(ScriptableRenderPassInput), typeof(FlagsAttribute));
            Notes.Note($"inputs:{input}:{flags}:{CameraRenderType.Overlay}:{CameraOverrideOption.UsePipelineSettings}:{RenderingMode.Forward}:{StoreActionsOptimization.Store}");
        }
    }
}
