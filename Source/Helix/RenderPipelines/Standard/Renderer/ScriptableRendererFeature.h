#pragma once

#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/RenderPipelines/Core/Context/ContextContainer.h>

// A feature of a renderer: an asset of its renderer data that adds passes to the frame (C++ classes below it, and C#
// classes below its C# class).
namespace helix::rendering::universal
{
    class ScriptableRenderer;

    // The frame as the features see it (the reference's compatibility views over the frame data).
    struct RenderingData
    {
        ContextContainer* frameData = nullptr;
    };
    struct CameraData
    {
        ContextContainer* frameData = nullptr;
    };

    HX_CLASS(Namespace = "HelixEngine.Rendering.Universal")
    class ScriptableRendererFeature : public engine::ScriptableObject
    {
    public:
        // Whether the renderer runs the feature.
        HX_PROPERTY(ReadOnly) bool isActive() const { return active_; }
        HX_FUNCTION() void SetActive(bool active) { active_ = active; }

        // Its passes are made (when it is enabled).
        virtual void Create() {}
        // Its passes for a camera's frame, queued into the renderer.
        virtual void AddRenderPasses(ScriptableRenderer& renderer, RenderingData& renderingData)
        {
            (void)renderer, (void)renderingData;
        }
        // Before the camera is culled.
        virtual void OnCameraPreCull(ScriptableRenderer& renderer, const CameraData& cameraData) { (void)renderer, (void)cameraData; }
        // What Create made goes.
        virtual void Dispose(bool disposing) { (void)disposing; }
        HX_FUNCTION(Name = "DisposeNative", Internal) void Dispose();

        // The renderer's calls: a C# feature's C# functions, else the C++ ones.
        void CallCreate();
        void CallAddRenderPasses(ScriptableRenderer& renderer, RenderingData& renderingData);
        void CallOnCameraPreCull(ScriptableRenderer& renderer, const CameraData& cameraData);

        // C#'s function for C# features: of (the C# object's GC handle, the call: 0 Create, 1 AddRenderPasses, 2
        // OnCameraPreCull, 3 Dispose, the renderer's id, the frame data's id).
        HX_FUNCTION(Internal) static void SetManaged(u64 call);

    private:
        void OnEnable();
        bool IsCSharp() const;

        HX_FIELD(SerializeField, Name = "m_Active") bool active_ = true;
    };
}
