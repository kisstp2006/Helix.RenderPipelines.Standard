#include <Helix/RenderPipelines/Standard/Renderer/PostProcessData.h>

#include <Helix/Engine/Object/Object.h>

namespace helix::rendering::universal
{
    using namespace engine;

    namespace
    {
        // The default data's shaders by field.
        struct DefaultShader
        {
            PostProcessShader field;
            const char* name;
        };
        constexpr DefaultShader kDefaultShaders[] = {
            {PostProcessShader::LutBuilderLdr, "Hidden/Universal Render Pipeline/LutBuilderLdr"},
            {PostProcessShader::LutBuilderHdr, "Hidden/Universal Render Pipeline/LutBuilderHdr"},
            {PostProcessShader::Bloom, "Hidden/Universal Render Pipeline/Bloom"},
            {PostProcessShader::UberPost, "Hidden/Universal Render Pipeline/UberPost"},
        };
    }

    PostProcessData* PostProcessData::Default()
    {
        static EntityId id;
        if (auto* d = EntityIdToObject<PostProcessData>(id))
            return d;
        auto* d = static_cast<PostProcessData*>(ScriptableObject::CreateInstance(scripting::TypeOf<PostProcessData>()));
        if (!d)
            return nullptr;
        d->SetName("DefaultPostProcessData");
        d->SetPersistent(true);
        d->hideFlags = HideFlags::HideAndDontSave;
        for (const DefaultShader& s : kDefaultShaders)
            d->SetShaderAt(i32(s.field), Shader::Find(s.name));
        id = d->GetEntityId();
        return d;
    }

    Shader* PostProcessData::ShaderAt(i32 index) const
    {
        return index >= 0 && u32(index) < kPostProcessShaderCount ? shaders_.shaders[index].Get() : nullptr;
    }

    void PostProcessData::SetShaderAt(i32 index, Shader* shader)
    {
        if (index >= 0 && u32(index) < kPostProcessShaderCount)
            shaders_.shaders[index] = PPtr<Shader>(shader);
    }

    Array<PPtr<Texture2D>>* PostProcessData::List(i32 list)
    {
        return list == 0 ? &textures_.blueNoise16LTex : list == 1 ? &textures_.filmGrainTex : nullptr;
    }

    i32 PostProcessData::TextureListCount(i32 list) const
    {
        auto* l = const_cast<PostProcessData*>(this)->List(list);
        return l ? i32(l->Size()) : 0;
    }

    Texture2D* PostProcessData::TextureListAt(i32 list, i32 index) const
    {
        auto* l = const_cast<PostProcessData*>(this)->List(list);
        return l && index >= 0 && u32(index) < l->Size() ? (*l)[u32(index)].Get() : nullptr;
    }

    void PostProcessData::SetTextureListCount(i32 list, i32 count)
    {
        if (auto* l = List(list))
            l->Resize(u32(Max(count, 0)));
    }

    void PostProcessData::SetTextureListAt(i32 list, i32 index, Texture2D* texture)
    {
        auto* l = List(list);
        if (l && index >= 0 && u32(index) < l->Size())
            (*l)[u32(index)] = PPtr<Texture2D>(texture);
    }

    Texture2D* PostProcessData::TextureAt(i32 index) const
    {
        return index == 0 ? textures_.smaaAreaTex.Get() : index == 1 ? textures_.smaaSearchTex.Get() : nullptr;
    }

    void PostProcessData::SetTextureAt(i32 index, Texture2D* texture)
    {
        if (index == 0)
            textures_.smaaAreaTex = PPtr<Texture2D>(texture);
        else if (index == 1)
            textures_.smaaSearchTex = PPtr<Texture2D>(texture);
    }
}
