import Foundation
import Network
import Combine
import ToastSaverCore

@MainActor
final class MixerConnection: ObservableObject {

    @Published private(set) var connectionState: ConnectionState = .disconnected

    enum ConnectionState: Equatable {
        case disconnected, connecting, connected, failed(String)
    }

    let model: MixerModel
    let host: String

    private var connection: NWConnection?
    private var keepaliveTimer: Timer?
    private var pendingFaderRead: (path: String, continuation: CheckedContinuation<Float?, Never>)?

    init(model: MixerModel, host: String) {
        self.model = model
        self.host = host
    }

    // MARK: - Lifecycle

    /// Open the UDP socket, send the model's handshake packet, wait up to 2 s
    /// for any reply. Sets connectionState = .connected on success, .failed on timeout.
    func connect() async {
        connectionState = .connecting
        makeConnection()
        let ping = OSCMessage(model.handshakePath)
        send(Data(ping.bytes))
        do { try await Task.sleep(for: .seconds(2)) } catch {}
        if case .connecting = connectionState {
            connectionState = .failed("No reply from \(host):\(model.port)")
        }
    }

    /// Close the NWConnection and stop the keepalive timer.
    func disconnect() {
        keepaliveTimer?.invalidate()
        keepaliveTimer = nil
        connection?.cancel()
        connection = nil
        connectionState = .disconnected
    }

    // MARK: - TEQ control

    /// Send /fx/{slot}/par/{par:02d} ,f {geqFloat(dB)}. Fire-and-forget.
    func sendNotch(slot: Int, par: Int, dB: Float) {
        send(Data(OSCMessage.geqBand(slot: slot, par: par, dB: dB).bytes))
    }

    /// Restore a TEQ band to flat (0 dB → geqFloat = 0.5).
    func sendFlat(slot: Int, par: Int) {
        sendNotch(slot: slot, par: par, dB: 0.0)
    }

    // MARK: - Fader control

    /// Query the current fader value for a bus. Returns raw OSC float [0, 1], or nil on timeout.
    func readFader(bus: Int) async -> Float? {
        let path = "/bus/\(bus)/mix/fader"
        return await withCheckedContinuation { continuation in
            pendingFaderRead = (path, continuation)
            send(Data(OSCMessage(path).bytes))
            Task {
                try? await Task.sleep(for: .seconds(1))
                if let pending = self.pendingFaderRead, pending.path == path {
                    self.pendingFaderRead = nil
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    /// Set a bus fader to a raw OSC float [0, 1].
    func writeFader(bus: Int, value: Float) {
        send(Data(OSCMessage("/bus/\(bus)/mix/fader", .float(value)).bytes))
    }

    // MARK: - Private

    private func makeConnection() {
        let endpoint = NWEndpoint.hostPort(
            host: NWEndpoint.Host(host),
            port: NWEndpoint.Port(rawValue: model.port)!
        )
        connection = NWConnection(to: endpoint, using: .udp)
        connection?.stateUpdateHandler = { [weak self] state in
            Task { @MainActor in
                switch state {
                case .ready:
                    self?.connectionState = .connected
                    self?.startKeepalive()
                case .failed(let e):
                    self?.connectionState = .failed(e.localizedDescription)
                default:
                    break
                }
            }
        }
        connection?.start(queue: .global(qos: .userInteractive))
        startReceiving()
    }

    private func startKeepalive() {
        send(Data(OSCMessage.xremote().bytes))
        keepaliveTimer = Timer.scheduledTimer(withTimeInterval: 9, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in
                self?.send(Data(OSCMessage.xremote().bytes))
            }
        }
    }

    private func startReceiving() {
        connection?.receive(minimumIncompleteLength: 1, maximumLength: 65535) { [weak self] data, _, isComplete, error in
            Task { @MainActor [weak self] in
                guard let self else { return }
                if let data, !data.isEmpty { self.handleIncoming(data) }
                if !isComplete && error == nil { self.startReceiving() }
            }
        }
    }

    private func handleIncoming(_ data: Data) {
        // Any packet from the mixer counts as a successful handshake
        if case .connecting = connectionState {
            connectionState = .connected
            startKeepalive()
        }
        // Resolve a pending fader read if address matches
        guard let pending = pendingFaderRead,
              let addr = oscAddress(from: data),
              addr == pending.path,
              let value = oscFloat(from: data) else { return }
        pendingFaderRead = nil
        pending.continuation.resume(returning: value)
    }

    private func send(_ data: Data) {
        connection?.send(content: data, completion: .idempotent)
    }

    // MARK: - OSC parsing

    private func oscAddress(from data: Data) -> String? {
        guard let null = data.firstIndex(of: 0) else { return nil }
        return String(bytes: data[..<null], encoding: .utf8)
    }

    private func oscFloat(from data: Data) -> Float? {
        // Skip address string (null-terminated, padded to 4-byte boundary)
        guard let addrNull = data.firstIndex(of: 0) else { return nil }
        var pos = (addrNull + 4) & ~3
        // Skip type tag string (null-terminated, padded to 4-byte boundary)
        guard pos < data.count, let tagNull = data[pos...].firstIndex(of: 0) else { return nil }
        let tagLen = tagNull - pos
        pos = pos + ((tagLen + 4) & ~3)
        // Read big-endian Float32
        guard pos + 4 <= data.count else { return nil }
        let bits = (UInt32(data[pos]) << 24) | (UInt32(data[pos + 1]) << 16)
                 | (UInt32(data[pos + 2]) << 8) | UInt32(data[pos + 3])
        return Float(bitPattern: bits)
    }
}
