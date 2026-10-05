import Combine
import ToastSaverCore

final class DetectorViewModel: ObservableObject {
    let geqBins: [Int]

    private let detector: FeedbackDetector
    private let notchController: NotchController
    private var baseline: BaselineTracker
    private var cancellable: AnyCancellable?
    private let binCount: Int

    init(
        fftEngine: FFTEngine,
        notchController: NotchController,
        threshold: Float = 20,
        confirmFrames: Int = 3
    ) {
        self.geqBins = fftEngine.geqBins
        self.binCount = fftEngine.fftSize / 2
        self.notchController = notchController
        self.detector = FeedbackDetector(
            threshold: threshold,
            confirmFrames: confirmFrames,
            narrowSkip: 2,
            narrowSpan: 3,
            narrowMinDB: 10.0
        )
        self.baseline = BaselineTracker(binCount: fftEngine.fftSize / 2, initial: -60)
    }

    func start(audio: AudioCaptureEngine) {
        cancellable = audio.$spectrum
            .filter { !$0.isEmpty }
            .sink { [weak self] spectrum in
                self?.process(spectrum: spectrum)
            }
    }

    func stop() {
        cancellable = nil
        detector.reset()
    }

    private func process(spectrum: [Float]) {
        baseline.update(with: spectrum, alpha: 0.005)
        let notchedPars = Set(
            notchController.parActive.enumerated().compactMap { index, active in
                active ? index + 1 : nil
            }
        )
        let detected = detector.process(
            bins: spectrum,
            baseline: baseline.baseline,
            notchedPars: notchedPars
        )
        notchController.update(detectedPar: detected)
    }
}
