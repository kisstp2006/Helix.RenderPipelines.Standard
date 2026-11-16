#pragma once

#include <Helix/Core/Strings/String.h>
#include <Helix/Engine/Rendering/Pipeline/CommandBuffer.h>
#include <Helix/RenderPipelines/Core/Context/ContextContainer.h>
#include <Helix/RenderPipelines/Core/RenderGraph/RenderGraph.h>
#include <Helix/RenderPipelines/Standard/Settings/PipelineSettings.h>

// A pass of a renderer: recorded on the frame's render graph at its event. C++ passes override RecordRenderGraph; a
// C# pass is an object of a C# class below this class's C# class, recorded through its C# RecordRenderGraph.
namespace helix::rendering::universal
{
    HX_CLASS(Root, Owned, Namespace = "HelixEngine.Rendering.Universal")
    class ScriptableRenderPass
    {
    public:
        HX_FUNCTION(Constructor) ScriptableRenderPass();
        virtual ~ScriptableRenderPass();
        ScriptableRenderPass(const ScriptableRenderPass&) = delete;
        ScriptableRenderPass& operator=(const ScriptableRenderPass&) = delete;

        // Where in the frame it runs (passes of one event keep the order they were queued in).
        HX_PROPERTY() RenderPassEvent renderPassEvent() const { return event_; }
        void SetRenderPassEvent(RenderPassEvent value) { event_ = value; }
        // The camera textures it reads: the renderer makes them.
        HX_PROPERTY(ReadOnly) ScriptableRenderPassInput input() const { return input_; }
        HX_FUNCTION() void ConfigureInput(ScriptableRenderPassInput passInput) { input_ = passInput; }
        // Whether it needs the camera drawn into an intermediate texture (it reads or writes the camera's color).
        HX_PROPERTY() bool requiresIntermediateTexture() const { return requiresIntermediate_; }
        void SetRequiresIntermediateTexture(bool value) { requiresIntermediate_ = value; }
        HX_PROPERTY() String passName() const { return name_; }
        void SetPassName(StringView value) { name_ = String(value); }

        // Its passes on the frame's graph (C++ passes override it).
        virtual void RecordRenderGraph(RenderGraph& graph, ContextContainer& frameData);
        // The camera's frame ended (what the pass set for it goes).
        virtual void OnCameraCleanup(engine::CommandBuffer& cmd) { (void)cmd; }

        // The renderer's calls: a C# pass's C# functions, else the C++ ones.
        void Record(RenderGraph& graph, ContextContainer& frameData);
        void CameraCleanup(engine::CommandBuffer& cmd);

        // C#'s functions for C# passes: of (the C# object's GC handle, the graph's id, the frame data's id) and (the GC
        // handle, the command buffer's id); each answers whether the object is of a C# class.
        HX_FUNCTION(Internal) static void SetManaged(u64 record, u64 cleanup);

        u64 Id() const { return id_; }
        u64& Wrapper() { return wrapper_; }
        static ScriptableRenderPass* Find(u64 id);
        virtual scripting::TypeId HelixType() const;

    private:
        RenderPassEvent event_ = RenderPassEvent::AfterRenderingOpaques;
        ScriptableRenderPassInput input_ = ScriptableRenderPassInput::None;
        bool requiresIntermediate_ = false;
        String name_;
        u64 id_ = 0;
        u64 wrapper_ = 0;
    };
}

template <>
struct helix::scripting::ObjectHandles<helix::rendering::universal::ScriptableRenderPass>
{
    static u64 ToId(const rendering::universal::ScriptableRenderPass* p) { return p->Id(); }
    static rendering::universal::ScriptableRenderPass* FromId(u64 id) { return rendering::universal::ScriptableRenderPass::Find(id); }
    static TypeId TypeOf(const rendering::universal::ScriptableRenderPass* p) { return p->HelixType(); }
    static u64& WrapperOf(rendering::universal::ScriptableRenderPass* p) { return p->Wrapper(); }
};
