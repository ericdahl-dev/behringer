import SwiftUI
import ToastSaverCore

struct ContentView: View {
    @AppStorage("xr18_ip")      private var xr18IP: String = "192.168.0.64"
    @AppStorage("fx_slot")      private var fxSlot: Int = 4
    @AppStorage("threshold_db") private var thresholdDb: Double = 20
    @AppStorage("cut_db")       private var cutDb: Double = -6
    @AppStorage("release_sec")  private var releaseSec: Double = 10

    @StateObject private var audio: AudioCaptureEngine
    @StateObject private var connection: XR18Connection
    @StateObject private var notchController: NotchController
    @StateObject private var detector: DetectorViewModel

    @State private var showConfig = false

    init() {
        let defaults = UserDefaults.standard
        let ip = defaults.string(forKey: "xr18_ip") ?? "192.168.0.64"
        let slot = defaults.integer(forKey: "fx_slot") == 0 ? 4 : defaults.integer(forKey: "fx_slot")
        let cut = defaults.double(forKey: "cut_db") == 0 ? -6.0 : defaults.double(forKey: "cut_db")
        let release = defaults.double(forKey: "release_sec") == 0 ? 10.0 : defaults.double(forKey: "release_sec")
        let thr = defaults.double(forKey: "threshold_db") == 0 ? 20.0 : defaults.double(forKey: "threshold_db")

        let fft = FFTEngine()
        let audioEngine = AudioCaptureEngine(fftEngine: fft)
        let conn = XR18Connection(host: ip)
        let notch = NotchController(
            connection: conn,
            fxSlot: slot,
            cutDb: Float(cut),
            releaseSeconds: Float(release)
        )
        let det = DetectorViewModel(
            fftEngine: fft,
            notchController: notch,
            threshold: Float(thr)
        )
        _audio = StateObject(wrappedValue: audioEngine)
        _connection = StateObject(wrappedValue: conn)
        _notchController = StateObject(wrappedValue: notch)
        _detector = StateObject(wrappedValue: det)
    }

    var body: some View {
        VStack(spacing: 0) {
            headerBar
            SpectrumView(
                spectrum: audio.spectrum,
                geqBins: detector.geqBins,
                parActive: notchController.parActive,
                threshold: Float(-thresholdDb)
            )
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .onTapGesture { notchController.restoreAll() }
            frequencyRuler
            notchMarkerRow
            statusBar
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color.black)
        .ignoresSafeArea(edges: .bottom)
        .preferredColorScheme(.dark)
        .sheet(isPresented: $showConfig) { ConfigView() }
        .task { await startAll() }
        .onDisappear { stopAll() }
    }

    // MARK: - Sub-views

    private var headerBar: some View {
        HStack {
            Text("Toast Saver")
                .font(.headline)
                .foregroundColor(.white)
            Spacer()
            Button {
                showConfig = true
            } label: {
                Image(systemName: "gear")
                    .foregroundColor(.white)
            }
            Circle()
                .fill(connectionColor)
                .frame(width: 8, height: 8)
            Text(connectionLabel)
                .font(.caption)
                .foregroundColor(connectionColor)
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(Color.black)
    }

    private var frequencyRuler: some View {
        GeometryReader { geo in
            ZStack(alignment: .topLeading) {
                Text("20Hz")
                    .font(.caption2)
                    .foregroundColor(.gray)
                    .position(x: 20, y: 8)
                let oneKhzFraction: CGFloat = detector.geqBins.count > 17
                    ? CGFloat(detector.geqBins[17]) / CGFloat(max(audio.spectrum.count - 1, 1))
                    : 0.5
                Text("1kHz")
                    .font(.caption2)
                    .foregroundColor(.gray)
                    .position(x: geo.size.width * oneKhzFraction, y: 8)
                Text("20kHz")
                    .font(.caption2)
                    .foregroundColor(.gray)
                    .position(x: geo.size.width - 25, y: 8)
            }
        }
        .frame(height: 16)
    }

    private var notchMarkerRow: some View {
        GeometryReader { geo in
            ForEach(0..<31, id: \.self) { j in
                if notchController.parActive[j] && j < detector.geqBins.count {
                    let fraction = CGFloat(detector.geqBins[j]) / CGFloat(max(audio.spectrum.count - 1, 1))
                    Text("▼")
                        .font(.caption2)
                        .foregroundColor(.red)
                        .position(x: geo.size.width * fraction, y: 8)
                }
            }
        }
        .frame(height: 16)
    }

    private var statusBar: some View {
        VStack(spacing: 4) {
            let activeIndices = notchController.parActive.enumerated()
                .filter(\.element)
                .map { $0.offset + 1 }
            if activeIndices.isEmpty {
                Text("No active notches")
                    .font(.caption)
                    .foregroundColor(.gray)
            } else {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack {
                        ForEach(activeIndices, id: \.self) { par in
                            Text("Par \(par)")
                                .font(.caption)
                                .padding(.horizontal, 6)
                                .padding(.vertical, 2)
                                .background(Color.red.opacity(0.3))
                                .cornerRadius(4)
                        }
                    }
                    .padding(.horizontal)
                }
            }
            Text("Slot:\(fxSlot)  thr:\(Int(thresholdDb))dB  cut:\(Int(cutDb))dB  rel:\(Int(releaseSec))s")
                .font(.caption2)
                .foregroundColor(.gray)
            Text(audio.inputSourceName)
                .font(.caption2)
                .foregroundColor(.gray)
        }
        .padding(.vertical, 8)
        .background(Color.black)
    }

    // MARK: - Helpers

    private var connectionColor: Color {
        switch connection.state {
        case .connected:    return .green
        case .connecting:   return .yellow
        case .failed:       return .red
        case .disconnected: return .gray
        }
    }

    private var connectionLabel: String {
        switch connection.state {
        case .connected:    return "LIVE"
        case .connecting:   return "..."
        case .failed:       return "ERR"
        case .disconnected: return "OFF"
        }
    }

    // MARK: - Lifecycle

    private func startAll() async {
        await connection.connect()
        try? await audio.start()
        detector.start(audio: audio)
    }

    private func stopAll() {
        detector.stop()
        notchController.restoreAll()
        audio.stop()
        connection.disconnect()
    }
}
