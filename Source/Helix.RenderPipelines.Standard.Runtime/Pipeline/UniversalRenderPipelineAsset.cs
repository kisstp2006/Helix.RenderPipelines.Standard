using System;

namespace HelixEngine.Rendering.Universal
{
    // What the asset's C# class adds to its bindings: RenderPipelineAsset's members (the engine renders with the C++
    // pipeline; CreatePipeline makes its C# view, RenderPipelineManager.currentPipeline), the global settings and the
    // constants.
    public partial class UniversalRenderPipelineAsset : ISerializationCallbackReceiver
    {
        public const int k_MinLutSize = 16;
        public const int k_MaxLutSize = 65;

        /// <summary>The C# pipeline of the C++ one the engine renders with: RenderPipelineManager.currentPipeline.</summary>
        protected override RenderPipeline CreatePipeline() => new UniversalRenderPipeline(this);

        /// <summary>The pipeline's global settings: the registered ones, else new ones; their default volume profile
        /// goes to the C++ pipeline.</summary>
        protected override void EnsureGlobalSettings()
        {
            UniversalRenderPipelineGlobalSettings settings = UniversalRenderPipelineGlobalSettings.Ensure();
            globalVolumeProfileNative = settings.defaultVolumeProfileSettings?.volumeProfile;
        }

        protected override bool requiresCompatibleRenderPipelineGlobalSettings => true;

        public override Type pipelineType => typeof(UniversalRenderPipeline);

        public override Material defaultMaterial => defaultMaterialNative;

        public override Shader defaultShader => defaultShaderNative;

        public override Material defaultParticleMaterial => defaultParticleMaterialNative;

        public override Material defaultLineMaterial => defaultLineMaterialNative;

        public override string renderPipelineShaderTag => UniversalRenderPipeline.k_ShaderTagName;

        /// <summary>The renderer data the cameras draw with (by index; the default one first unless another is set).</summary>
        public ReadOnlySpan<ScriptableRendererData> rendererDataList => rendererDataArray;
    }
}
