// Proactive ring-out controller — pure Swift port of cli/ringout_logic.c.
// No Apple framework imports; builds and tests on Linux.

// MARK: - Config

/// Immutable configuration for one ring-out run.
public struct RingoutConfig {
    /// Gain the bus starts at (dB). Captured from the bus fader before the run.
    public var startGainDB: Float
    /// Hard upper bound — never command gain above this.
    public var ceilingDB: Float
    /// Per-step raise increment and back-off amount (dB). Must be > 0.
    public var stepDB: Float
    /// Stop once (currentGain − startGain) >= this. <= 0 disables the check.
    public var targetMarginDB: Float
    /// Stop after placing this many notches.
    public var maxNotches: Int
    /// dB above baseline required to flag a potential feedback bin.
    public var thresholdDB: Float
    /// Notch depth applied per ring (negative dB, e.g. -6).
    public var cutDB: Float
    /// Consecutive frames a ring must persist before a notch fires.
    public var confirmFrames: Int
    /// Frames to wait at current gain after any raise or notch before analysing.
    public var settleFrames: Int
    /// Consecutive ring-free frames required before raising gain.
    public var stableFrames: Int
    /// Narrowness gate: bins to skip immediately adjacent to the peak.
    public var narrowSkip: Int
    /// Narrowness gate: bins to sample beyond the skip zone on each side.
    public var narrowSpan: Int
    /// Narrowness gate: peak must exceed neighbour average by at least this many dB.
    public var narrowMinDB: Float

    public init(
        startGainDB: Float, ceilingDB: Float, stepDB: Float,
        targetMarginDB: Float = 0, maxNotches: Int = 8,
        thresholdDB: Float = 20.0, cutDB: Float = -6.0,
        confirmFrames: Int = 3, settleFrames: Int = 10, stableFrames: Int = 10,
        narrowSkip: Int = 2, narrowSpan: Int = 3, narrowMinDB: Float = 10.0
    ) {
        self.startGainDB    = startGainDB
        self.ceilingDB      = ceilingDB
        self.stepDB         = stepDB
        self.targetMarginDB = targetMarginDB
        self.maxNotches     = maxNotches
        self.thresholdDB    = thresholdDB
        self.cutDB          = cutDB
        self.confirmFrames  = confirmFrames
        self.settleFrames   = settleFrames
        self.stableFrames   = stableFrames
        self.narrowSkip     = narrowSkip
        self.narrowSpan     = narrowSpan
        self.narrowMinDB    = narrowMinDB
    }
}

// MARK: - Action / Reason

/// Why a ring-out run terminated.
public enum RingoutDoneReason: Equatable {
    /// Next raise would exceed the gain ceiling.
    case ceilingReached
    /// (currentGain − startGain) >= targetMarginDB.
    case targetMarginReached
    /// Notch budget exhausted.
    case maxNotchesReached
    /// An already-notched band rang again (runaway feedback).
    case runaway
}

/// One decision returned by `RingoutController.step()` per FFT frame.
public enum RingoutAction: Equatable {
    /// Stay at current gain; continue observing.
    case hold
    /// Command the bus fader to this gain (dB).
    case raiseGain(to: Float)
    /// Feedback detected; back off to this gain (dB).
    case backOff(to: Float)
    /// Place a notch on this GEQ par (1–31) at the given cut depth.
    case placeNotch(par: Int, cutDB: Float)
    /// Run finished cleanly — notches should be kept.
    case done(reason: RingoutDoneReason, margin: Float)
    /// Run terminated abnormally — notches should be reverted.
    case abort(reason: RingoutDoneReason)
}

// MARK: - Controller

/// Stateful proactive ring-out controller. Call `step()` once per FFT frame.
/// The controller never performs I/O — the caller executes returned actions.
public final class RingoutController {
    public let config: RingoutConfig

    /// Gain currently commanded on the bus (dB).
    public private(set) var currentGainDB: Float
    /// Number of notches placed so far.
    public private(set) var notchCount: Int
    /// Headroom achieved at termination (set once done/abort is returned).
    public private(set) var marginDB: Float

    // Internal phase
    private enum Phase { case analyze, settle, done }
    private var phase: Phase
    private var settleCount: Int
    private var stableCount: Int
    private var confirmCount: Int
    private var confirmBin: Int       // GEQ-level index 0..30; -1 = none
    private var pendingBackoff: Bool
    private var placed: [Bool]        // placed[i] = true if par i+1 is notched
    private var terminalAction: RingoutAction?  // cached once done/abort fires

    public init(config: RingoutConfig) {
        self.config        = config
        currentGainDB      = config.startGainDB
        notchCount         = 0
        marginDB           = 0
        phase              = .analyze
        settleCount        = 0
        stableCount        = 0
        confirmCount       = 0
        confirmBin         = -1
        pendingBackoff     = false
        placed             = [Bool](repeating: false, count: 31)
        terminalAction     = nil
    }

    /// Process one FFT frame.
    ///
    /// - Parameter geqLevels: 31-element array of dBFS values, one per GEQ band
    ///   (`geqLevels[i]` = GEQ par `i+1`). Produced by sampling FFT output at
    ///   `FFTEngine.geqBins[i]` for each band.
    /// - Parameter baseline: 31-element slow-moving average of `geqLevels`.
    ///   Update between frames with `BaselineTracker`.
    /// - Returns: one `RingoutAction` for this frame.
    public func step(geqLevels: [Float], baseline: [Float]) -> RingoutAction {
        // Already done — replay the terminal action.
        if phase == .done { return terminalAction! }

        // A notch placed last step: back the gain off and re-settle.
        if pendingBackoff {
            pendingBackoff = false
            currentGainDB -= config.stepDB
            if currentGainDB < config.startGainDB { currentGainDB = config.startGainDB }
            phase        = .settle
            settleCount  = config.settleFrames
            return .backOff(to: currentGainDB)
        }

        // Settling: hold until the system stabilises after a raise or notch.
        if phase == .settle {
            if settleCount > 0 { settleCount -= 1; return .hold }
            // Settle complete — fall through and analyse this frame.
            phase        = .analyze
            confirmCount = 0
            confirmBin   = -1
            stableCount  = 0
        }

        // Analyse: look for a narrow ring.
        guard let peakIndex = PeakDetector.detectPeak(
            bins: geqLevels, baseline: baseline, threshold: config.thresholdDB)
        else {
            return handleQuietFrame()
        }

        let narrow = NarrownessFilter.isNarrowPeak(
            bins: geqLevels, peakBin: peakIndex,
            skip: config.narrowSkip, span: config.narrowSpan, minDB: config.narrowMinDB)

        guard narrow else { return handleQuietFrame() }

        // An already-notched par is ringing again — the notch isn't holding; abort.
        if placed[peakIndex] {
            return terminal(.abort(reason: .runaway))
        }

        // Accumulate confirmation frames.
        if peakIndex == confirmBin {
            confirmCount += 1
        } else {
            confirmBin   = peakIndex
            confirmCount = 1
        }
        stableCount = 0

        guard confirmCount >= config.confirmFrames else { return .hold }

        // Confirmed: place the notch.
        placed[peakIndex] = true
        notchCount       += 1
        confirmCount      = 0
        confirmBin        = -1
        pendingBackoff    = true
        return .placeNotch(par: peakIndex + 1, cutDB: config.cutDB)
    }

    /// Reset to initial state for reuse with the same config.
    public func reset() {
        currentGainDB  = config.startGainDB
        notchCount     = 0
        marginDB       = 0
        phase          = .analyze
        settleCount    = 0
        stableCount    = 0
        confirmCount   = 0
        confirmBin     = -1
        pendingBackoff = false
        placed         = [Bool](repeating: false, count: 31)
        terminalAction = nil
    }

    // MARK: - Private helpers

    private func handleQuietFrame() -> RingoutAction {
        confirmCount = 0
        confirmBin   = -1
        stableCount += 1

        guard stableCount >= config.stableFrames else { return .hold }
        stableCount = 0

        // Check stop conditions before committing to another raise.
        if notchCount >= config.maxNotches {
            return terminal(.done(reason: .maxNotchesReached, margin: achievedMargin()))
        }
        if config.targetMarginDB > 0 && achievedMargin() >= config.targetMarginDB {
            return terminal(.done(reason: .targetMarginReached, margin: achievedMargin()))
        }
        if currentGainDB + config.stepDB > config.ceilingDB {
            return terminal(.done(reason: .ceilingReached, margin: achievedMargin()))
        }

        // Safe to raise.
        currentGainDB += config.stepDB
        phase          = .settle
        settleCount    = config.settleFrames
        return .raiseGain(to: currentGainDB)
    }

    private func achievedMargin() -> Float {
        currentGainDB - config.startGainDB
    }

    @discardableResult
    private func terminal(_ action: RingoutAction) -> RingoutAction {
        phase          = .done
        marginDB       = achievedMargin()
        terminalAction = action
        return action
    }
}
