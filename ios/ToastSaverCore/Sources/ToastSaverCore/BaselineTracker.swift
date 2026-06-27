// Tracks a slow-moving per-bin baseline (the "normal" room spectrum) so the
// detector can spot a bin rising above it. Exponential smoothing, ported from
// cli/toast_logic.c (update_baseline).

public struct BaselineTracker {
    /// The current smoothed baseline, one value per RTA bin.
    public private(set) var baseline: [Float]

    public init(binCount: Int = 100, initial: Float = 0) {
        baseline = Array(repeating: initial, count: binCount)
    }

    public init(baseline: [Float]) {
        self.baseline = baseline
    }

    /// Pull each bin a fraction `alpha` toward the latest reading:
    /// `baseline[i] = (1-alpha)*baseline[i] + alpha*bins[i]`.
    public mutating func update(with bins: [Float], alpha: Float) {
        let n = min(baseline.count, bins.count)
        for i in 0..<n {
            baseline[i] = (1.0 - alpha) * baseline[i] + alpha * bins[i]
        }
    }
}
