#pragma once

#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Engine/Texture/Texture.h>

// The post processing resources of a forward renderer (the reference's PostProcessData): its shaders and textures in
// the reference's file layout. C# reaches them through PostProcessData.ShaderResources and TextureResources.
namespace helix::rendering::universal
{
    // The shaders' fields in their order (the reference's ShaderResources).
    inline constexpr StringView kPostProcessShaderNames[] = {"stopNanPS", "subpixelMorphologicalAntialiasingPS", "gaussianDepthOfFieldPS",
                                                             "bokehDepthOfFieldPS", "cameraMotionBlurPS", "paniniProjectionPS", "lutBuilderLdrPS",
                                                             "lutBuilderHdrPS", "bloomPS", "temporalAntialiasingPS", "LensFlareDataDrivenPS",
                                                             "LensFlareScreenSpacePS", "scalingSetupPS", "easuPS", "uberPostPS", "finalPostPassPS"};
    inline constexpr u32 kPostProcessShaderCount = u32(sizeof(kPostProcessShaderNames) / sizeof(kPostProcessShaderNames[0]));

    // The shaders this pipeline has, by field (the default data finds them by name).
    enum class PostProcessShader : i32
    {
        LutBuilderLdr = 6,
        LutBuilderHdr = 7,
        Bloom = 8,
        UberPost = 14,
    };

    // The shaders in files (C# reaches them by index).
    HX_STRUCT(SerializedOnly)
    struct PostProcessShaderResources
    {
        static constexpr StringView kTypeName = "ShaderResources";
        engine::PPtr<engine::Shader> shaders[kPostProcessShaderCount];

        template <class V>
        void Transfer(V& v)
        {
            for (u32 i = 0; i < kPostProcessShaderCount; i++)
                v.Field(kPostProcessShaderNames[i], shaders[i]);
        }
    };

    // The textures in files.
    HX_STRUCT(SerializedOnly)
    struct PostProcessTextureResources
    {
        static constexpr StringView kTypeName = "TextureResources";
        Array<engine::PPtr<engine::Texture2D>> blueNoise16LTex = Array<engine::PPtr<engine::Texture2D>>();
        Array<engine::PPtr<engine::Texture2D>> filmGrainTex = Array<engine::PPtr<engine::Texture2D>>();
        engine::PPtr<engine::Texture2D> smaaAreaTex, smaaSearchTex;

        template <class V>
        void Transfer(V& v)
        {
            v.Field("blueNoise16LTex", blueNoise16LTex);
            v.Field("filmGrainTex", filmGrainTex);
            v.Field("smaaAreaTex", smaaAreaTex);
            v.Field("smaaSearchTex", smaaSearchTex);
        }
    };

    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class PostProcessData : public engine::ScriptableObject
    {
    public:
        engine::Shader* shader(PostProcessShader which) const { return ShaderAt(i32(which)); }

        // The data of renderer data made without one: the pipeline's shaders found by name (made once, kept).
        static PostProcessData* Default();

        // C#'s ShaderResources and TextureResources (fields by index; the texture lists 0 blue noise, 1 film grain;
        // the single textures 0 SMAA area, 1 SMAA search).
        HX_FUNCTION(Internal) engine::Shader* ShaderAt(i32 index) const;
        HX_FUNCTION(Internal) void SetShaderAt(i32 index, engine::Shader* shader);
        HX_FUNCTION(Internal) i32 TextureListCount(i32 list) const;
        HX_FUNCTION(Internal) engine::Texture2D* TextureListAt(i32 list, i32 index) const;
        HX_FUNCTION(Internal) void SetTextureListCount(i32 list, i32 count);
        HX_FUNCTION(Internal) void SetTextureListAt(i32 list, i32 index, engine::Texture2D* texture);
        HX_FUNCTION(Internal) engine::Texture2D* TextureAt(i32 index) const;
        HX_FUNCTION(Internal) void SetTextureAt(i32 index, engine::Texture2D* texture);

    private:
        Array<engine::PPtr<engine::Texture2D>>* List(i32 list);

        HX_FIELD(SerializeField, Name = "shaders") PostProcessShaderResources shaders_;
        HX_FIELD(SerializeField, Name = "textures") PostProcessTextureResources textures_;
    };
}

// The resources' file layout (the reference's nested fields).
namespace helix::serialization
{
    template <>
    struct TransferTraits<rendering::universal::PostProcessShaderResources>
    {
        static constexpr StringView kTypeName = "ShaderResources";
        template <class V>
        static void Transfer(V& v, rendering::universal::PostProcessShaderResources& r)
        {
            r.Transfer(v);
        }
    };

    template <>
    struct TransferTraits<rendering::universal::PostProcessTextureResources>
    {
        static constexpr StringView kTypeName = "TextureResources";
        template <class V>
        static void Transfer(V& v, rendering::universal::PostProcessTextureResources& r)
        {
            r.Transfer(v);
        }
    };
}
