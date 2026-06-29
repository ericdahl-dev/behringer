import AVFoundation
import Combine

final class AudioCaptureEngine: ObservableObject {
    /// Current power spectrum: fftSize/2 values in dBFS. Published on main thread.
    @Published private(set) var spectrum: [Float] = []

    /// Active input source description (e.g. "USB Audio" or "Built-in Microphone").
    @Published private(set) var inputSourceName: String = "—"

    /// True while capture is running.
    @Published private(set) var isRunning: Bool = false

    private var fftEngine: FFTEngine
    private let engine = AVAudioEngine()
    private var accumulator: [Float] = []
    private let fftQueue = DispatchQueue(label: "toast.fft", qos: .userInteractive)
    private var routeChangeObserver: Any?

    init(fftEngine: FFTEngine) {
        self.fftEngine = fftEngine
    }

    deinit {
        if let obs = routeChangeObserver {
            NotificationCenter.default.removeObserver(obs)
        }
    }

    /// Request microphone permission then start capture. Throws on denial or engine error.
    func start() async throws {
        if #available(iOS 17.0, *) {
            guard await AVAudioApplication.requestRecordPermission() else {
                throw CaptureError.permissionDenied
            }
        } else {
            let granted = await withCheckedContinuation { continuation in
                AVAudioSession.sharedInstance().requestRecordPermission { continuation.resume(returning: $0) }
            }
            guard granted else { throw CaptureError.permissionDenied }
        }
        try configureSession()
        installTap()
        try engine.start()
        observeRouteChanges()
        await MainActor.run { isRunning = true }
    }

    /// Stop capture and deactivate audio session.
    func stop() {
        if let obs = routeChangeObserver {
            NotificationCenter.default.removeObserver(obs)
            routeChangeObserver = nil
        }
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        DispatchQueue.main.async { [weak self] in self?.isRunning = false }
    }

    private func restart() async throws {
        stop()
        try await start()
    }

    private func configureSession() throws {
        let session = AVAudioSession.sharedInstance()
        // .measurement disables AGC and system noise reduction — required for accurate frequency response
        try session.setCategory(.record, mode: .measurement, options: [.allowBluetoothHFP])
        try session.setActive(true)

        let name: String
        if let usbInput = session.availableInputs?.first(where: { $0.portType == .usbAudio }) {
            try session.setPreferredInput(usbInput)
            name = usbInput.portName
        } else {
            name = session.currentRoute.inputs.first?.portName ?? "Built-in Microphone"
        }
        DispatchQueue.main.async { [weak self] in self?.inputSourceName = name }

        try session.setPreferredSampleRate(48000)
        try session.setPreferredIOBufferDuration(1024 / 48000)  // ~21 ms
    }

    private func installTap() {
        let inputNode = engine.inputNode
        let format = inputNode.inputFormat(forBus: 0)
        // Rebuild FFT engine to match the actual session sample rate (may settle at 44100)
        fftEngine = FFTEngine(fftSize: 2048, sampleRate: format.sampleRate)
        inputNode.installTap(onBus: 0, bufferSize: 512, format: format) { [weak self] buffer, _ in
            self?.handleBuffer(buffer)
        }
    }

    private func observeRouteChanges() {
        routeChangeObserver = NotificationCenter.default.addObserver(
            forName: AVAudioSession.routeChangeNotification,
            object: nil,
            queue: .main
        ) { [weak self] notification in
            guard let reason = notification.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
                  let changeReason = AVAudioSession.RouteChangeReason(rawValue: reason) else { return }
            switch changeReason {
            case .newDeviceAvailable, .oldDeviceUnavailable:
                Task { try? await self?.restart() }
            default:
                break
            }
        }
    }

    private func handleBuffer(_ buffer: AVAudioPCMBuffer) {
        guard let channelData = buffer.floatChannelData?[0] else { return }
        let count = Int(buffer.frameLength)
        accumulator.append(contentsOf: UnsafeBufferPointer(start: channelData, count: count))

        while accumulator.count >= fftEngine.fftSize {
            let block = Array(accumulator.prefix(fftEngine.fftSize))
            accumulator.removeFirst(fftEngine.fftSize)
            fftQueue.async { [weak self] in
                guard let self else { return }
                let result = self.fftEngine.process(block)
                DispatchQueue.main.async { self.spectrum = result }
            }
        }
    }
}

enum CaptureError: Error {
    case permissionDenied
}
