// MARK: - Stateless peak detector (port of detect_peak from cli/toast_logic.c)

/// Finds the loudest RTA bin that rises strictly more than `threshold` dB above
/// its tracked baseline. Usable on any `[Float]` — 100-bin RTA or 31-bin geqLevels.
public enum PeakDetector {
    /// Returns the index of the highest-amplitude qualifying bin, or nil if none.
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

// MARK: - Stateful single-bin feedback detector (reactive mode)

/// Wraps `PeakDetector` + `NarrownessFilter` with a per-par confirmation counter.
/// Designed for the reactive (post-ring-out) protection pipeline: input is the
/// 100-bin RTA from the mixer; peaks are mapped to GEQ pars via `GEQMap`.
///
/// Call `process()` once per RTA frame from a single-threaded context.
public final class FeedbackDetector {
    public let threshold: Float
    public let confirmFrames: Int
    public let narrowSkip: Int
    public let narrowSpan: Int
    public let narrowMinDB: Float

    private var lastPar: Int = 0     // 1–31; 0 = none tracked
    private var confirmCount: Int = 0

    public init(
        threshold: Float = 20.0,
        confirmFrames: Int = 3,
        narrowSkip: Int = 2,
        narrowSpan: Int = 3,
        narrowMinDB: Float = 10.0
    ) {
        self.threshold     = threshold
        self.confirmFrames = confirmFrames
        self.narrowSkip    = narrowSkip
        self.narrowSpan    = narrowSpan
        self.narrowMinDB   = narrowMinDB
    }

    /// Process one RTA frame.
    ///
    /// - Parameter bins: RTA bins (e.g. 100 for XR18, 50 for X32).
    /// - Parameter baseline: Slow-moving average of `bins`, same length.
    /// - Parameter notchedPars: GEQ pars already notched; a confirmed par in this
    ///   set is suppressed and the counter is reset.
    /// - Returns: GEQ par 1–31 when the same par has been confirmed for
    ///   `confirmFrames` consecutive frames and is not in `notchedPars`, else nil.
    public func process(
        bins: [Float],
        baseline: [Float],
        notchedPars: Set<Int> = []
    ) -> Int? {
        guard let peakBin = PeakDetector.detectPeak(
            bins: bins, baseline: baseline, threshold: threshold)
        else {
            resetConfirm()
            return nil
        }

        guard NarrownessFilter.isNarrowPeak(
            bins: bins, peakBin: peakBin,
            skip: narrowSkip, span: narrowSpan, minDB: narrowMinDB)
        else {
            resetConfirm()
            return nil
        }

        let par = GEQMap.parForBin(peakBin)

        if par == lastPar {
            confirmCount += 1
        } else {
            lastPar      = par
            confirmCount = 1
        }

        guard confirmCount >= confirmFrames else { return nil }

        // Already notched — suppress and reset so a new frequency can accumulate.
        if notchedPars.contains(par) {
            resetConfirm()
            return nil
        }

        resetConfirm()
        return par
    }

    /// Reset confirmation state (e.g. after placing a notch or changing gain level).
    public func reset() { resetConfirm() }

    private func resetConfirm() {
        lastPar      = 0
        confirmCount = 0
    }
}
