// Distinguishes a narrow feedback ring from broadband content (e.g. a vocal
// formant): a real ring towers over its neighbours a few bins out. Ported from
// cli/toast_logic.c (is_narrow_peak).

public enum NarrownessFilter {
    /// True if `bins[peakBin]` is at least `minDB` above the average of the
    /// neighbour bins sampled `skip+1 … skip+span` positions away on each side
    /// (clamped to the array). Needs ≥1 neighbour on each side, else false.
    public static func isNarrowPeak(bins: [Float], peakBin: Int,
                                    skip: Int = 2, span: Int = 3, minDB: Float = 10.0) -> Bool {
        let n = bins.count
        var sum: Float = 0
        var lcnt = 0, rcnt = 0
        for i in stride(from: peakBin - skip - span, through: peakBin - skip - 1, by: 1) where i >= 0 {
            sum += bins[i]; lcnt += 1
        }
        for i in stride(from: peakBin + skip + 1, through: peakBin + skip + span, by: 1) where i < n {
            sum += bins[i]; rcnt += 1
        }
        guard lcnt >= 1, rcnt >= 1 else { return false }
        return (bins[peakBin] - sum / Float(lcnt + rcnt)) >= minDB
    }
}
