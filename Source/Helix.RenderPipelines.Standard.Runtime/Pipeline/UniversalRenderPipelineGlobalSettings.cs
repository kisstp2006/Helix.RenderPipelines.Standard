using System;
using System.Collections.Generic;

namespace HelixEngine.Rendering.Universal
{
    /// <summary>The default volume profile of the pipeline (VolumeManager's global default; the asset's profile is the quality one).</summary>
    [Serializable]
    public class URPDefaultVolumeProfileSettings : IRenderPipelineGraphicsSettings
    {
        enum Version
        {
            Initial = 0,
        }

        [SerializeField] Version m_Version = Version.Initial;
        [SerializeField] VolumeProfile m_VolumeProfile;

        public int version => (int)m_Version;

        public VolumeProfile volumeProfile
        {
            get => m_VolumeProfile;
            set => m_VolumeProfile = value;
        }
    }

    /// <summary>The render graph's settings of the pipeline (the render graph path is the only one).</summary>
    [Serializable]
    public class RenderGraphSettings : IRenderPipelineGraphicsSettings
    {
        enum Version
        {
            Initial = 0,
        }

        [SerializeField] Version m_Version = Version.Initial;

        public int version => (int)m_Version;
    }

    /// <summary>
    /// The pipeline's global settings: registered for UniversalRenderPipeline (by the asset's EnsureGlobalSettings
    /// when none is), found through GraphicsSettings.GetRenderPipelineSettings.
    /// </summary>
    public class UniversalRenderPipelineGlobalSettings : RenderPipelineGlobalSettings
    {
        [SerializeField] URPDefaultVolumeProfileSettings m_DefaultVolumeProfile = new URPDefaultVolumeProfileSettings();
        [SerializeField] RenderGraphSettings m_RenderGraph = new RenderGraphSettings();

        List<IRenderPipelineGraphicsSettings> m_List;

        internal URPDefaultVolumeProfileSettings defaultVolumeProfileSettings => m_DefaultVolumeProfile;

        protected override List<IRenderPipelineGraphicsSettings> settingsList => m_List ??= new List<IRenderPipelineGraphicsSettings> { m_DefaultVolumeProfile, m_RenderGraph };

        /// <summary>The settings registered for the pipeline (null before EnsureGlobalSettings made or found them).</summary>
        public static UniversalRenderPipelineGlobalSettings instance => GraphicsSettings.GetSettingsForRenderPipeline<UniversalRenderPipeline>() as UniversalRenderPipelineGlobalSettings;

        // The registered settings, made when there are none.
        internal static UniversalRenderPipelineGlobalSettings Ensure()
        {
            UniversalRenderPipelineGlobalSettings settings = instance;
            if (settings == null)
            {
                settings = CreateInstance<UniversalRenderPipelineGlobalSettings>();
                settings.name = "UniversalRenderPipelineGlobalSettings";
                GraphicsSettings.RegisterRenderPipelineSettings<UniversalRenderPipeline>(settings);
            }
            return settings;
        }
    }
}
