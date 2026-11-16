using System;

namespace HelixEngine.Rendering.Universal
{
    /// <summary>
    /// The post processing resources of a forward renderer: its shaders and textures. The lists and fields are the
    /// native data's (ShaderResources and TextureResources read and write it).
    /// </summary>
    public partial class PostProcessData
    {
        [Serializable]
        public sealed class ShaderResources
        {
            readonly PostProcessData m_Data;

            internal ShaderResources(PostProcessData data) => m_Data = data;

            Shader Get(int index) => m_Data.ShaderAt(index);
            void Set(int index, Shader value) => m_Data.SetShaderAt(index, value);

            public Shader stopNanPS { get => Get(0); set => Set(0, value); }
            public Shader subpixelMorphologicalAntialiasingPS { get => Get(1); set => Set(1, value); }
            public Shader gaussianDepthOfFieldPS { get => Get(2); set => Set(2, value); }
            public Shader bokehDepthOfFieldPS { get => Get(3); set => Set(3, value); }
            public Shader cameraMotionBlurPS { get => Get(4); set => Set(4, value); }
            public Shader paniniProjectionPS { get => Get(5); set => Set(5, value); }
            public Shader lutBuilderLdrPS { get => Get(6); set => Set(6, value); }
            public Shader lutBuilderHdrPS { get => Get(7); set => Set(7, value); }
            public Shader bloomPS { get => Get(8); set => Set(8, value); }
            public Shader temporalAntialiasingPS { get => Get(9); set => Set(9, value); }
            public Shader LensFlareDataDrivenPS { get => Get(10); set => Set(10, value); }
            public Shader LensFlareScreenSpacePS { get => Get(11); set => Set(11, value); }
            public Shader scalingSetupPS { get => Get(12); set => Set(12, value); }
            public Shader easuPS { get => Get(13); set => Set(13, value); }
            public Shader uberPostPS { get => Get(14); set => Set(14, value); }
            public Shader finalPostPassPS { get => Get(15); set => Set(15, value); }
        }

        [Serializable]
        public sealed class TextureResources
        {
            readonly PostProcessData m_Data;

            internal TextureResources(PostProcessData data) => m_Data = data;

            Texture2D[] GetList(int list)
            {
                var textures = new Texture2D[m_Data.TextureListCount(list)];
                for (int i = 0; i < textures.Length; i++)
                    textures[i] = m_Data.TextureListAt(list, i);
                return textures;
            }

            void SetList(int list, Texture2D[] textures)
            {
                m_Data.SetTextureListCount(list, textures?.Length ?? 0);
                for (int i = 0; textures != null && i < textures.Length; i++)
                    m_Data.SetTextureListAt(list, i, textures[i]);
            }

            public Texture2D[] blueNoise16LTex { get => GetList(0); set => SetList(0, value); }
            public Texture2D[] filmGrainTex { get => GetList(1); set => SetList(1, value); }
            public Texture2D smaaAreaTex { get => m_Data.TextureAt(0); set => m_Data.SetTextureAt(0, value); }
            public Texture2D smaaSearchTex { get => m_Data.TextureAt(1); set => m_Data.SetTextureAt(1, value); }
        }

        public ShaderResources shaders => new ShaderResources(this);
        public TextureResources textures => new TextureResources(this);
    }
}
