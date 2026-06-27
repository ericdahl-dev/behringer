// Finds a candidate feedback bin: the loudest RTA bin that rises strictly more
// than `threshold` dB above its tracked baseline. Ported from
// cli/toast_logic.c (detect_peak).

public enum FeedbackDetector {
    /// Returns the index of the highest-amplitude bin whose excess over its
    /// baseline strictly exceeds `threshold`, or nil if no bin qualifies.
    public static func detectPeak(bins: [Float], baseline: [Float], threshold: Float) -> Int? {
        var best = -1
        var bestVal: Float = -999.0
        let n = min(bins.count, baseline.count)
        for i in 0..<n {
            let excess = bins[i] - baseline[i]
            if excess > threshold && bins[i] > bestVal {
                bestVal = bins[i]
                best = i
            }
        }
        return best < 0 ? nil : best
    }
}
