namespace QuickPlayerCore.PackedPresenters
{
    public class SamplingParams
    {
        public StandardBitDepth BitDepth { get; set; }
        public StandardSamplingRates SamplingRate { get; set; }
        public bool IsLossless { get; set; }
        public uint TotalBitrate { get; set; }
        public Codec Codec { get; set; }
    }
}
