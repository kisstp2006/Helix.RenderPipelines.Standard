#include <Helix/RenderPipelines/Standard/Passes/ScriptableRenderPass.h>

#include <Helix/Core/Containers/HashMap.h>

namespace helix::rendering::universal
{
    namespace
    {
        struct Passes
        {
            HashMap<u64, ScriptableRenderPass*> all;
            u64 next = 0;
            u8 (*record)(u64 wrapper, u64 graph, u64 frame) = nullptr;
            u8 (*cleanup)(u64 wrapper, u64 cmd) = nullptr;
        };

        Passes& P()
        {
            static Passes p;
            return p;
        }
    }

    ScriptableRenderPass::ScriptableRenderPass() : id_(++P().next) { P().all.Set(id_, this); }

    ScriptableRenderPass::~ScriptableRenderPass()
    {
        scripting::ReleaseWrapper(wrapper_);
        P().all.Remove(id_);
    }

    ScriptableRenderPass* ScriptableRenderPass::Find(u64 id)
    {
        ScriptableRenderPass* const* p = P().all.Find(id);
        return p ? *p : nullptr;
    }

    scripting::TypeId ScriptableRenderPass::HelixType() const { return scripting::TypeOf<ScriptableRenderPass>(); }

    void ScriptableRenderPass::RecordRenderGraph(RenderGraph& graph, ContextContainer& frameData) { (void)graph, (void)frameData; }

    void ScriptableRenderPass::Record(RenderGraph& graph, ContextContainer& frameData)
    {
        if (wrapper_ && P().record && P().record(wrapper_, graph.Id(), frameData.Id()))
            return;
        RecordRenderGraph(graph, frameData);
    }

    void ScriptableRenderPass::CameraCleanup(engine::CommandBuffer& cmd)
    {
        if (wrapper_ && P().cleanup && P().cleanup(wrapper_, cmd.Id()))
            return;
        OnCameraCleanup(cmd);
    }

    void ScriptableRenderPass::SetManaged(u64 record, u64 cleanup)
    {
        P().record = reinterpret_cast<u8 (*)(u64, u64, u64)>(uintptr_t(record));
        P().cleanup = reinterpret_cast<u8 (*)(u64, u64)>(uintptr_t(cleanup));
    }
}
