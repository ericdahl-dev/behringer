import Foundation
import Combine
import ToastSaverCore

@MainActor
final class RingoutSession: ObservableObject {

    // MARK: - Published state

    @Published private(set) var phase: Phase = .idle
    @Published private(set) var currentGainDB: Float = 0
    @Published private(set) var notchCount: Int = 0
    @Published private(set) var activeNotches: [Bool] = Array(repeating: false, count: 31)
    @Published private(set) var logLines: [String] = []

    // MARK: - Types

    enum Phase: Equatable {
        case idle
        case calibrating(framesCollected: Int, framesNeeded: Int)
        case analyzing
        case settling
        case waitingForConfirm(nextGainDB: Float)
        case done(reason: RingoutDoneReason, marginDB: Float)
        case aborted(reason: String)
    }

    struct Config {
        var mixerModel: MixerModel
        var mixerHost: String
        var bus: Int
        var fxSlot: Int
        var rtaChannel: Int
        var ringoutConfig: RingoutConfig
        var supervised: Bool
        var calibrationFrames: Int
    }

    // MARK: - Dependencies

    private let connection: MixerConnection
    private let audio: AudioCaptureEngine
    private let fft: FFTEngine

    // MARK: - Runtime state

    private var rampTask: Task<Void, Never>?
    private var pendingConfirm: CheckedContinuation<Bool, Never>?
    private var capturedFaderValue: Float = 0
    private var needsCleanup = false
    private var activeConfig: Config?

    // MARK: - Logging

    private static let logFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "HH:mm:ss.SSS"
        return f
    }()

    // MARK: - Init

    init(connection: MixerConnection, audio: AudioCaptureEngine, fft: FFTEngine) {
        self.connection = connection
        self.audio = audio
        self.fft = fft
    }

    // MARK: - Public API

    func start(config: Config) {
        pendingConfirm?.resume(returning: false)
        pendingConfirm = nil
        rampTask?.cancel()
        activeConfig = config
        needsCleanup = false
        capturedFaderValue = 0
        phase = .idle
        currentGainDB = 0
        notchCount = 0
        activeNotches = Array(repeating: false, count: 31)
        logLines = []
        rampTask = Task { [weak self] in
            guard let self else { return }
            await self.runSession(config: config)
        }
    }

    func stop() {
        pendingConfirm?.resume(returning: false)
        pendingConfirm = nil
        rampTask?.cancel()
        rampTask = nil
        if let cfg = activeConfig, needsCleanup {
            for i in 0..<31 where activeNotches[i] {
                connection.sendFlat(slot: cfg.fxSlot, par: i + 1)
            }
            activeNotches = Array(repeating: false, count: 31)
            connection.writeFader(bus: cfg.bus, value: capturedFaderValue)
            needsCleanup = false
        }
        if case .done = phase { return }
        phase = .aborted(reason: "Stopped by user")
        appendLog("Session stopped by user")
    }

    func confirmRaise() {
        pendingConfirm?.resume(returning: true)
        pendingConfirm = nil
    }

    // MARK: - Session loop

    private func runSession(config: Config) async {
        // Step 1: Capture fader before touching anything.
        guard let faderValue = await connection.readFader(bus: config.bus) else {
            phase = .aborted(reason: "Could not read fader for bus \(config.bus)")
            appendLog("Abort: could not read fader for bus \(config.bus)")
            return
        }
        guard !Task.isCancelled else { return }

        capturedFaderValue = faderValue
        needsCleanup = true
        let startGainDB = faderFloatToDB(faderValue)
        currentGainDB = startGainDB

        // Step 2: Calibration + ramp loop driven by the audio spectrum publisher.
        var calibrationData: [[Float]] = []
        calibrationData.reserveCapacity(config.calibrationFrames)
        phase = .calibrating(framesCollected: 0, framesNeeded: config.calibrationFrames)

        var tracker: BaselineTracker? = nil
        var controller: RingoutController? = nil
        var remainingSettleFrames = 0

        for await spectrum in audio.$spectrum.values {
            guard !Task.isCancelled else { return }
            guard !spectrum.isEmpty else { continue }

            let geqLevels = fft.geqBins.map { bin in
                bin < spectrum.count ? spectrum[bin] : -60.0
            }

            // ---- Calibration phase ----
            if tracker == nil {
                calibrationData.append(geqLevels)
                let count = calibrationData.count
                phase = .calibrating(framesCollected: count, framesNeeded: config.calibrationFrames)

                if count >= config.calibrationFrames {
                    var baselineValues = [Float](repeating: 0.0, count: 31)
                    for i in 0..<31 {
                        baselineValues[i] = calibrationData.reduce(0.0) { $0 + $1[i] } / Float(count)
                    }
                    tracker = BaselineTracker(baseline: baselineValues)

                    var ringoutConfig = config.ringoutConfig
                    ringoutConfig.startGainDB = startGainDB
                    controller = RingoutController(config: ringoutConfig)

                    phase = .analyzing
                    appendLog(
                        "Calibration complete: \(count) frames, " +
                        "start gain \(String(format: "%.1f", startGainDB)) dB"
                    )
                }
                continue
            }

            // ---- Ramp phase ----
            tracker!.update(with: geqLevels, alpha: 0.005)
            let action = controller!.step(geqLevels: geqLevels, baseline: tracker!.baseline)
            currentGainDB = controller!.currentGainDB

            switch action {
            case .hold:
                if remainingSettleFrames > 0 {
                    remainingSettleFrames -= 1
                    phase = .settling
                } else {
                    phase = .analyzing
                }

            case .raiseGain(let targetDB):
                if config.supervised {
                    phase = .waitingForConfirm(nextGainDB: targetDB)
                    let confirmed = await withCheckedContinuation { (cont: CheckedContinuation<Bool, Never>) in
                        pendingConfirm = cont
                    }
                    guard confirmed, !Task.isCancelled else { return }
                }
                connection.writeFader(bus: config.bus, value: faderDBToFloat(targetDB))
                remainingSettleFrames = config.ringoutConfig.settleFrames
                phase = .settling
                appendLog("Raise gain to \(String(format: "%.1f", targetDB)) dB")

            case .backOff(let targetDB):
                connection.writeFader(bus: config.bus, value: faderDBToFloat(targetDB))
                remainingSettleFrames = config.ringoutConfig.settleFrames
                phase = .settling
                appendLog("Back off to \(String(format: "%.1f", targetDB)) dB")

            case .placeNotch(let par, let cutDB):
                connection.sendNotch(slot: config.fxSlot, par: par, dB: cutDB)
                activeNotches[par - 1] = true
                notchCount = controller!.notchCount
                appendLog("Notch par \(par) at \(String(format: "%.1f", cutDB)) dB")

            case .done(let reason, let margin):
                connection.writeFader(bus: config.bus, value: capturedFaderValue)
                needsCleanup = false
                phase = .done(reason: reason, marginDB: margin)
                appendLog("Done: \(reason), margin \(String(format: "%.1f", margin)) dB")
                return

            case .abort(let reason):
                for i in 0..<31 where activeNotches[i] {
                    connection.sendFlat(slot: config.fxSlot, par: i + 1)
                }
                activeNotches = Array(repeating: false, count: 31)
                connection.writeFader(bus: config.bus, value: capturedFaderValue)
                needsCleanup = false
                phase = .aborted(reason: String(describing: reason))
                appendLog("Abort: \(reason)")
                return
            }
        }
    }

    // MARK: - Logging

    private func appendLog(_ message: String) {
        let timestamp = Self.logFormatter.string(from: Date())
        logLines.append("[\(timestamp)] \(message)")
        if logLines.count > 200 {
            logLines.removeFirst(logLines.count - 200)
        }
    }
}
