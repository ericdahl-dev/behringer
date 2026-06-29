import Foundation
import Network
import Combine
import ToastSaverCore

final class XR18Connection: ObservableObject {
    @Published private(set) var state: ConnectionState = .disconnected

    enum ConnectionState {
        case disconnected, connecting, connected, failed(String)
    }

    private let host: String
    private let port: UInt16
    private var connection: NWConnection?
    private var keepaliveTimer: Timer?

    init(host: String, port: UInt16 = 10024) {
        self.host = host
        self.port = port
    }

    /// Send /xinfo and wait for reply. Sets state = .connected on success.
    func connect() async {
        await MainActor.run { state = .connecting }
        makeConnection()
        // UDP goes .ready immediately; wait up to 2 s for the state handler
        try? await Task.sleep(for: .seconds(2))
        guard case .connected = state else {
            await MainActor.run { state = .failed("No reply from \(host)") }
            return
        }
        send(OSCMessage.xinfo())
    }

    /// Stop connection and cancel keepalive timer.
    func disconnect() {
        keepaliveTimer?.invalidate()
        keepaliveTimer = nil
        connection?.cancel()
        connection = nil
        DispatchQueue.main.async { self.state = .disconnected }
    }

    /// Send /fx/{slot}/par/{par:02d} ,f {value} — fire and forget.
    func sendTEQBand(slot: Int, par: Int, value: Float) {
        let pp = par < 10 ? "0\(par)" : "\(par)"
        send(OSCMessage("/fx/\(slot)/par/\(pp)", .float(value)))
    }

    /// Send a pre-built OSCMessage.
    func send(_ message: OSCMessage) {
        send(Data(message.bytes))
    }

    /// Send raw OSC bytes.
    func send(_ data: Data) {
        connection?.send(content: data, completion: .idempotent)
    }

    // MARK: - Private

    private func makeConnection() {
        let endpoint = NWEndpoint.hostPort(
            host: NWEndpoint.Host(host),
            port: NWEndpoint.Port(rawValue: port)!
        )
        connection = NWConnection(to: endpoint, using: .udp)
        connection?.stateUpdateHandler = { [weak self] newState in
            DispatchQueue.main.async {
                switch newState {
                case .ready:
                    self?.state = .connected
                    self?.startKeepalive()
                case .failed(let err):
                    self?.state = .failed(err.localizedDescription)
                default:
                    break
                }
            }
        }
        connection?.start(queue: .global(qos: .userInteractive))
    }

    private func startKeepalive() {
        send(OSCMessage.xremote())
        keepaliveTimer = Timer.scheduledTimer(withTimeInterval: 9, repeats: true) { [weak self] _ in
            self?.send(OSCMessage.xremote())
        }
    }
}
