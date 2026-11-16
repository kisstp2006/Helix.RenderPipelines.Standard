namespace HelixEngine.Rendering.Universal
{
    public partial class UniversalAdditionalCameraData
    {
        /// <summary>The layers of the volumes blended for this camera.</summary>
        public LayerMask volumeLayerMask
        {
            get => volumeLayerMaskBits;
            set => volumeLayerMaskBits = value;
        }

        /// <summary>The camera's own volume stack (made when its volumes are blended for it alone; null until then).</summary>
        public VolumeStack volumeStack
        {
            get => global::Helix.Interop.VolumeInterop.StackOf(volumeStackId);
            set => volumeStackId = global::Helix.Interop.VolumeInterop.IdOf(value);
        }
    }

    /// <summary>The pipeline's settings of a camera: the component beside it (added when it has none).</summary>
    public static class CameraExtensions
    {
        public static UniversalAdditionalCameraData GetUniversalAdditionalCameraData(this Camera camera) => AdditionalDataNative.OfCamera(camera);
    }

    /// <summary>The pipeline's settings of a light: the component beside it (added when it has none).</summary>
    public static class LightExtensions
    {
        public static UniversalAdditionalLightData GetUniversalAdditionalLightData(this Light light) => AdditionalDataNative.OfLight(light);
    }
}
