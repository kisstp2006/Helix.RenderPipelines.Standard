using System;

namespace HelixEngine.Rendering.Universal
{
    /// <summary>
    /// The Standard pipeline. The engine makes and runs it in C++ while a UniversalRenderPipelineAsset is set; this C#
    /// object is RenderPipelineManager.currentPipeline then (made by the asset's CreatePipeline, disposed with the C++
    /// one), and C# reads the pipeline's constants, asset and global settings here.
    /// </summary>
    public sealed partial class UniversalRenderPipeline : RenderPipeline
    {
        /// <summary>The value of the RenderPipeline tag of the SubShaders it draws.</summary>
        public const string k_ShaderTagName = "UniversalPipeline";

        readonly UniversalRenderPipelineAsset m_Asset;

        internal UniversalRenderPipeline(UniversalRenderPipelineAsset asset) => m_Asset = asset;

        /// <summary>The pipeline asset in use when it is this pipeline's (else null).</summary>
        public static UniversalRenderPipelineAsset asset => UniversalRenderPipelineNative.Asset();

        /// <summary>The pipeline's global settings.</summary>
        public override RenderPipelineGlobalSettings defaultSettings => UniversalRenderPipelineGlobalSettings.instance;

        /// <summary>The most lights per object.</summary>
        public static int maxPerObjectLights => 8;

        /// <summary>The most additional lights of a camera (fewer on OpenGL ES and WebGL 2).</summary>
        public static int maxVisibleAdditionalLights => UniversalRenderPipelineNative.MaxVisibleAdditionalLights();

        public const float minRenderScale = 0.1f;
        public const float maxRenderScale = 2.0f;
        public const float maxShadowBias = 10.0f;
        public const int maxNumIterationsEnclosingSphere = 1000;

        /// <summary>Runs in C++ (the engine renders with the native pipeline).</summary>
        protected override void Render(ScriptableRenderContext context, Camera[] cameras) =>
            throw new InvalidOperationException("The Standard pipeline renders in C++.");

        public override string ToString() => m_Asset != null ? $"UniversalRenderPipeline ({m_Asset.name})" : "UniversalRenderPipeline";
    }
}
