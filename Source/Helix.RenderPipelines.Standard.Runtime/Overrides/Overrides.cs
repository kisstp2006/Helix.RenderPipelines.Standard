using System;

namespace HelixEngine.Rendering.Universal
{
    // The C# side of the post processing volume components: their parameters are C++ members (the generated
    // properties), the enum parameters' C# classes are here.

    [Serializable]
    public sealed class DownscaleParameter : VolumeParameter<BloomDownscaleMode>
    {
        public DownscaleParameter(BloomDownscaleMode value, bool overrideState = false) : base(value, overrideState) { }
    }

    [Serializable]
    public sealed class TonemappingModeParameter : VolumeParameter<TonemappingMode>
    {
        public TonemappingModeParameter(TonemappingMode value, bool overrideState = false) : base(value, overrideState) { }
    }

    public sealed partial class Bloom : IPostProcessComponent
    {
    }

    public sealed partial class Tonemapping : IPostProcessComponent
    {
    }

    public sealed partial class ColorAdjustments : IPostProcessComponent
    {
    }
}
