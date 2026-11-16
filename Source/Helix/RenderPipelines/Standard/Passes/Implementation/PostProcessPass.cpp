#include <Helix/RenderPipelines/Standard/Passes/PostProcessPass.h>

#include <Helix/Engine/Rendering/SystemInfo.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Engine/Texture/RenderTexture.h>
#include <Helix/RenderPipelines/Core/Utilities/Blitter.h>
#include <Helix/RenderPipelines/Standard/Frame/FrameData.h>
#include <Helix/RenderPipelines/Standard/Overrides/Overrides.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipelineAsset.h>
#include <Helix/RenderPipelines/Standard/Renderer/PostProcessData.h>

#include <cmath>

namespace helix::rendering::universal
{
    using namespace engine;

    namespace
    {
        // The material of a shader of the data (made again when the data's shader changes).
        Material* Make(PPtr<Material>& held, Shader* shader)
        {
            Material* m = held.Get();
            if (m && m->shader() != shader)
            {
                Object::DestroyImmediate(m);
                m = nullptr;
            }
            if (!m && shader)
                m = Material::Create(shader);
            held = PPtr<Material>(m);
            return m;
        }

        void SetKeyword(Material& m, const char* keyword, bool on)
        {
            if (on)
                m.EnableKeyword(keyword);
            else
                m.DisableKeyword(keyword);
        }

        template <class T>
        const T* Component(const VolumeStack* stack)
        {
            return stack ? stack->GetComponent<T>() : nullptr;
        }

        // A full screen pass of a material from a source (or none) into a target, with globals set before it.
        struct FullScreenData
        {
            RenderGraph* graph = nullptr;
            Material* material = nullptr;
            i32 pass = 0;
            TextureHandle source, low, bloom, lut;
            Texture* dirt = nullptr;
            Vector4 vectors[4];
            i32 ids[4] = {};
            u32 vectorCount = 0;
            f32 dirtIntensity = 0;
        };

        void SetVector(FullScreenData& d, const char* name, Vector4 v)
        {
            d.ids[d.vectorCount] = Shader::PropertyToID(name);
            d.vectors[d.vectorCount++] = v;
        }

        void Draw(FullScreenData& d, RasterGraphContext& ctx)
        {
            static const i32 lowId = Shader::PropertyToID("_SourceTexLowMip"), bloomId = Shader::PropertyToID("_Bloom_Texture"),
                             lutId = Shader::PropertyToID("_InternalLut"), dirtId = Shader::PropertyToID("_LensDirt_Texture"),
                             dirtIntensityId = Shader::PropertyToID("_LensDirt_Intensity");
            for (u32 i = 0; i < d.vectorCount; i++)
                ctx.cmd->SetGlobalVector(d.ids[i], d.vectors[i]);
            if (d.low.IsValid())
                if (RTHandle* h = d.graph->Resolve(d.low))
                    ctx.cmd->SetGlobalTexture(lowId, h->rt());
            if (d.bloom.IsValid())
                if (RTHandle* h = d.graph->Resolve(d.bloom))
                    ctx.cmd->SetGlobalTexture(bloomId, h->rt());
            if (d.lut.IsValid())
                if (RTHandle* h = d.graph->Resolve(d.lut))
                    ctx.cmd->SetGlobalTexture(lutId, h->rt());
            if (d.dirt)
            {
                ctx.cmd->SetGlobalTexture(dirtId, d.dirt);
                ctx.cmd->SetGlobalFloat(dirtIntensityId, d.dirtIntensity);
            }
            if (d.source.IsValid())
            {
                if (RTHandle* source = d.graph->Resolve(d.source))
                    Blitter::BlitTexture(*ctx.cmd, *source, Vector4(1, 1, 0, 0), d.material, d.pass);
            }
            else
                Blitter::BlitTexture(*ctx.cmd, Vector4(1, 1, 0, 0), d.material, d.pass);
        }

        // A pass drawing the material from source into target; setup adds its globals and other reads (the builder ends
        // with this call).
        template <class Setup>
        void AddFullScreen(RenderGraph& graph, const char* name, Material* material, i32 pass, TextureHandle source, TextureHandle target, Setup&& setup)
        {
            FullScreenData* data = nullptr;
            PassBuilder<FullScreenData> builder = graph.AddRasterRenderPass(name, data);
            data->graph = &graph;
            data->material = material;
            data->pass = pass;
            data->source = source;
            if (source.IsValid())
                builder.UseTexture(source);
            builder.SetRenderAttachment(target, 0, AccessFlags::WriteAll);
            builder.AllowGlobalStateModification(true);
            setup(*data, builder);
            builder.SetRenderFunc([](FullScreenData& d, RasterGraphContext& ctx) { Draw(d, ctx); });
        }

        Color Linear(Color c) { return c.linear(); }
    }

    GraphicsFormat HdrColorFormat(const UniversalRenderPipelineAsset* asset)
    {
        bool packed = !asset || asset->hdrColorBufferPrecision() == HDRColorBufferPrecision::_32Bits;
        if (packed && SystemInfo::SupportsRenderTextureFormat(RenderTextureFormat::RGB111110Float))
            return GraphicsFormat::B10G11R11_UFloatPack32;
        return GraphicsFormat::R16G16B16A16_SFloat;
    }

    // ------------------------------------------------------------------------------------------ the look up table

    ColorGradingLutPass::~ColorGradingLutPass()
    {
        for (Material* m : {ldr_.Get(), hdr_.Get()})
            if (m)
                Object::DestroyImmediate(m);
    }

    bool ColorGradingLutPass::HdrGrading(const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData)
    {
        return postData.gradingMode == ColorGradingMode::HighDynamicRange && cameraData.isHdrEnabled;
    }

    TextureHandle ColorGradingLutPass::Record(RenderGraph& graph, const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData,
                                              const VolumeStack* stack, const PostProcessData& resources)
    {
        bool hdr = HdrGrading(cameraData, postData);
        Material* material = hdr ? Make(hdr_, resources.shader(PostProcessShader::LutBuilderHdr)) : Make(ldr_, resources.shader(PostProcessShader::LutBuilderLdr));
        if (!material)
            return TextureHandle();
        i32 lutHeight = Max(2, postData.lutSize), lutWidth = lutHeight * lutHeight;
        TextureDesc desc(lutWidth, lutHeight);
        desc.colorFormat = hdr ? GraphicsFormat::R16G16B16A16_SFloat : GraphicsFormat::R8G8B8A8_UNorm;
        desc.filterMode = FilterMode::Bilinear;
        desc.wrapMode = TextureWrapMode::Clamp;
        desc.name = String("_InternalGradingLut");
        TextureHandle lut = graph.CreateTexture(desc);

        const ColorAdjustments* adjustments = Component<ColorAdjustments>(stack);
        const Tonemapping* tonemapping = Component<Tonemapping>(stack);
        TonemappingMode mode = tonemapping && tonemapping->IsActive() ? tonemapping->mode.value() : TonemappingMode::None;
        if (hdr)
        {
            SetKeyword(*material, "_TONEMAP_ACES", mode == TonemappingMode::ACES);
            SetKeyword(*material, "_TONEMAP_NEUTRAL", mode == TonemappingMode::Neutral);
        }
        Color filter = adjustments ? Linear(adjustments->colorFilter.value()) : Color(1, 1, 1, 1);
        f32 hue = adjustments ? adjustments->hueShift.value() / 360.0f : 0.0f;
        f32 saturation = adjustments ? adjustments->saturation.value() / 100.0f + 1.0f : 1.0f;
        f32 contrast = adjustments ? adjustments->contrast.value() / 100.0f + 1.0f : 1.0f;
        AddFullScreen(graph, "ColorGradingLUT", material, 0, TextureHandle(), lut, [&](FullScreenData& d, PassBuilder<FullScreenData>&) {
            SetVector(d, "_Lut_Params", Vector4(f32(lutHeight), 0.5f / f32(lutWidth), 0.5f / f32(lutHeight), f32(lutHeight) / f32(lutHeight - 1)));
            SetVector(d, "_ColorFilter", Vector4(filter.r, filter.g, filter.b, filter.a));
            SetVector(d, "_HueSatCon", Vector4(hue, saturation, contrast, 0));
        });
        return lut;
    }

    // ------------------------------------------------------------------------------------------ bloom and uber

    PostProcessPass::~PostProcessPass()
    {
        for (Material* m : {bloom_.Get(), uber_.Get()})
            if (m)
                Object::DestroyImmediate(m);
    }

    TextureHandle PostProcessPass::RecordBloom(RenderGraph& graph, const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData,
                                               const VolumeStack* stack, const PostProcessData& resources, TextureHandle source)
    {
        const Bloom* bloom = Component<Bloom>(stack);
        Material* material = Make(bloom_, resources.shader(PostProcessShader::Bloom));
        if (!bloom || !bloom->IsActive() || !material)
            return TextureHandle();
        // The chain: from half or a quarter of the camera, halved maxIterations times at most (down to 2 texels).
        i32 shift = bloom->downscale.value() == BloomDownscaleMode::Quarter ? 2 : 1;
        i32 tw = Max(1, cameraData.scaledWidth >> shift), th = Max(1, cameraData.scaledHeight >> shift);
        i32 iterations = i32(std::floor(std::log2(f32(Max(tw, th))) - 1.0f));
        i32 mipCount = Min(Max(iterations, 1), Min(bloom->maxIterations.value(), 16));
        SetKeyword(*material, "_BLOOM_HQ", bloom->highQualityFiltering.value());
        SetKeyword(*material, "_USE_FAST_SRGB_LINEAR_CONVERSION", postData.useFastSRGBLinearConversion);
        f32 threshold = mathf::GammaToLinearSpace(bloom->threshold.value());
        Vector4 params(mathf::Lerp(0.05f, 0.95f, bloom->scatter.value()), bloom->clamp.value(), threshold, threshold * 0.5f);

        GraphicsFormat format = HdrColorFormat(nullptr);
        TextureHandle down[16], up[16];
        for (i32 i = 0; i < mipCount; i++)
        {
            TextureDesc desc(Max(1, tw >> i), Max(1, th >> i));
            desc.colorFormat = format;
            desc.filterMode = FilterMode::Bilinear;
            desc.wrapMode = TextureWrapMode::Clamp;
            desc.name = String(Format("_BloomMipDown{}", i));
            down[i] = graph.CreateTexture(desc);
            desc.name = String(Format("_BloomMipUp{}", i));
            up[i] = graph.CreateTexture(desc);
        }
        auto pass = [&](const char* name, i32 index, TextureHandle from, TextureHandle to, TextureHandle low) {
            AddFullScreen(graph, name, material, index, from, to, [&](FullScreenData& d, PassBuilder<FullScreenData>& b) {
                SetVector(d, "_Params", params);
                if (low.IsValid())
                {
                    d.low = low;
                    b.UseTexture(low);
                }
            });
        };
        pass("Bloom Prefilter", 0, source, down[0], TextureHandle());
        for (i32 i = 1; i < mipCount; i++)
        {
            pass("Bloom Blur Horizontal", 1, down[i - 1], up[i], TextureHandle());
            pass("Bloom Blur Vertical", 2, up[i], down[i], TextureHandle());
        }
        for (i32 i = mipCount - 2; i >= 0; i--)
            pass("Bloom Upsample", 3, down[i], up[i], i == mipCount - 2 ? down[i + 1] : up[i + 1]);
        return mipCount > 1 ? up[0] : down[0];
    }

    TextureHandle PostProcessPass::Record(RenderGraph& graph, const UniversalCameraData& cameraData, const UniversalPostProcessingData& postData,
                                          const VolumeStack* stack, const PostProcessData& resources, TextureHandle source, TextureHandle lut)
    {
        Material* material = Make(uber_, resources.shader(PostProcessShader::UberPost));
        if (!material || !source.IsValid())
            return source;
        TextureHandle bloomTexture = RecordBloom(graph, cameraData, postData, stack, resources, source);
        const Bloom* bloom = Component<Bloom>(stack);
        const ColorAdjustments* adjustments = Component<ColorAdjustments>(stack);
        const Tonemapping* tonemapping = Component<Tonemapping>(stack);
        bool hdr = ColorGradingLutPass::HdrGrading(cameraData, postData);
        TonemappingMode mode = tonemapping && tonemapping->IsActive() ? tonemapping->mode.value() : TonemappingMode::None;
        Texture* dirt = bloom && bloomTexture.IsValid() ? EntityIdToObject<Texture>(bloom->dirtTexture.value()) : nullptr;
        bool useDirt = dirt && bloom->dirtIntensity.value() > 0.0f;
        bool hq = bloomTexture.IsValid() && bloom->highQualityFiltering.value();
        SetKeyword(*material, "_BLOOM_LQ", bloomTexture.IsValid() && !hq && !useDirt);
        SetKeyword(*material, "_BLOOM_HQ", hq && !useDirt);
        SetKeyword(*material, "_BLOOM_LQ_DIRT", bloomTexture.IsValid() && !hq && useDirt);
        SetKeyword(*material, "_BLOOM_HQ_DIRT", hq && useDirt);
        SetKeyword(*material, "_HDR_GRADING", hdr);
        SetKeyword(*material, "_TONEMAP_ACES", !hdr && mode == TonemappingMode::ACES);
        SetKeyword(*material, "_TONEMAP_NEUTRAL", !hdr && mode == TonemappingMode::Neutral);
        SetKeyword(*material, "_USE_FAST_SRGB_LINEAR_CONVERSION", postData.useFastSRGBLinearConversion);

        // The graded and tonemapped image is LDR (single sampled, the camera's size).
        TextureDesc desc = graph.GetTextureDesc(source);
        desc.name = String("_CameraColorAfterPostProcessing");
        desc.colorFormat = GraphicsFormat::R8G8B8A8_UNorm;
        desc.msaaSamples = MSAASamples::None;
        desc.clearBuffer = false;
        TextureHandle target = graph.CreateTexture(desc);
        i32 lutHeight = Max(2, postData.lutSize), lutWidth = lutHeight * lutHeight;
        f32 exposure = adjustments ? std::exp2(adjustments->postExposure.value()) : 1.0f;
        AddFullScreen(graph, "UberPost", material, 0, source, target, [&](FullScreenData& d, PassBuilder<FullScreenData>& b) {
            SetVector(d, "_Lut_Params", Vector4(1.0f / f32(lutWidth), 1.0f / f32(lutHeight), f32(lutHeight - 1), exposure));
            if (lut.IsValid())
            {
                d.lut = lut;
                b.UseTexture(lut);
            }
            if (!bloomTexture.IsValid())
                return;
            // The tint at luminance 1 (white when it is black).
            Color tint = Linear(bloom->tint.value());
            f32 luma = 0.2126729f * tint.r + 0.7151522f * tint.g + 0.0721750f * tint.b;
            tint = luma > 0.0f ? Color(tint.r / luma, tint.g / luma, tint.b / luma, tint.a) : Color(1, 1, 1, 1);
            SetVector(d, "_Bloom_Params", Vector4(bloom->intensity.value(), tint.r, tint.g, tint.b));
            d.bloom = bloomTexture;
            b.UseTexture(bloomTexture);
            if (!useDirt)
                return;
            // The dirt texture over the screen, cropped to keep its aspect.
            f32 dirtRatio = f32(dirt->width()) / f32(Max(1, dirt->height()));
            f32 screenRatio = f32(cameraData.scaledWidth) / f32(Max(1, cameraData.scaledHeight));
            Vector4 scaleOffset(1, 1, 0, 0);
            if (dirtRatio > screenRatio)
            {
                scaleOffset.x = screenRatio / dirtRatio;
                scaleOffset.z = (1.0f - scaleOffset.x) * 0.5f;
            }
            else if (screenRatio > dirtRatio)
            {
                scaleOffset.y = dirtRatio / screenRatio;
                scaleOffset.w = (1.0f - scaleOffset.y) * 0.5f;
            }
            SetVector(d, "_LensDirt_Params", scaleOffset);
            d.dirt = dirt;
            d.dirtIntensity = bloom->dirtIntensity.value();
        });
        return target;
    }
}
