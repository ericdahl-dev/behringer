/// Describes the wire-protocol differences between Behringer mixer families.
/// All fields are constant after construction.
public struct MixerModel: Equatable, Sendable {
    /// Human-readable name, e.g. "XR18" or "X32".
    public let name: String

    /// UDP port the mixer listens on.
    public let port: UInt16

    /// OSC address the mixer pushes for RTA meter data (e.g. "/meters/4").
    public let metersPath: String

    /// Number of RTA bins in a meters packet (100 for XR18, 50 for X32).
    public let rtaBinCount: Int

    /// OSC address for the identity handshake query (e.g. "/xinfo" or "/info").
    public let handshakePath: String

    /// Number of input channels (18 for XR18, 32 for X32).
    public let inputChannels: Int

    /// Number of mix buses (6 for XR18, 16 for X32).
    public let busCount: Int

    /// Number of FX processor slots (4 for XR18, 8 for X32).
    public let fxSlots: Int

    public init(name: String, port: UInt16, metersPath: String, rtaBinCount: Int,
                handshakePath: String, inputChannels: Int, busCount: Int, fxSlots: Int) {
        self.name          = name
        self.port          = port
        self.metersPath    = metersPath
        self.rtaBinCount   = rtaBinCount
        self.handshakePath = handshakePath
        self.inputChannels = inputChannels
        self.busCount      = busCount
        self.fxSlots       = fxSlots
    }
}

public extension MixerModel {
    /// Behringer XR18 / XAir series.
    static let xr18 = MixerModel(
        name:          "XR18",
        port:          10024,
        metersPath:    "/meters/4",
        rtaBinCount:   100,
        handshakePath: "/xinfo",
        inputChannels: 18,
        busCount:      6,
        fxSlots:       4
    )

    /// Behringer X32 / M32 series.
    static let x32 = MixerModel(
        name:          "X32",
        port:          10023,
        metersPath:    "/meters/15",
        rtaBinCount:   50,
        handshakePath: "/info",
        inputChannels: 32,
        busCount:      16,
        fxSlots:       8
    )

    /// Returns the model matching a case-insensitive name string, or nil if unrecognised.
    static func named(_ name: String) -> MixerModel? {
        switch name.lowercased() {
        case "xr18", "xair": return .xr18
        case "x32", "m32":   return .x32
        default:             return nil
        }
    }
}
