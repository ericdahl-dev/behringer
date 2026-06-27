// Minimal OSC 1.0 message encoder — no Foundation, no Apple frameworks. Builds
// the UDP payload sent to the XR18 (big-endian, 4-byte-aligned strings/args).

public enum OSCArgument {
    case float(Float)
    case int32(Int32)
    case string(String)

    var tag: Character {
        switch self {
        case .float:  return "f"
        case .int32:  return "i"
        case .string: return "s"
        }
    }
}

public struct OSCMessage {
    /// The encoded packet, ready to send over UDP.
    public let bytes: [UInt8]

    public init(_ address: String, _ arguments: OSCArgument...) {
        var out: [UInt8] = []
        OSCMessage.appendString(address, to: &out)

        var tags = ","
        for a in arguments { tags.append(a.tag) }
        OSCMessage.appendString(tags, to: &out)

        for a in arguments {
            switch a {
            case .float(let v):  OSCMessage.appendBE(v.bitPattern, to: &out)
            case .int32(let v):  OSCMessage.appendBE(UInt32(bitPattern: v), to: &out)
            case .string(let v): OSCMessage.appendString(v, to: &out)
            }
        }
        bytes = out
    }

    /// Convenience: a single GEQ-band cut for an FX-slot graphic EQ.
    /// `/fx/{slot}/par/{par:02} ,f {geqFloat(dB)}`.
    public static func geqBand(slot: Int, par: Int, dB: Float) -> OSCMessage {
        let pp = par < 10 ? "0\(par)" : "\(par)"
        return OSCMessage("/fx/\(slot)/par/\(pp)", .float(GEQMap.geqFloat(forDB: dB)))
    }

    // OSC string: UTF-8, null-terminated, zero-padded to a 4-byte boundary.
    private static func appendString(_ s: String, to out: inout [UInt8]) {
        out.append(contentsOf: Array(s.utf8))
        out.append(0)
        while out.count % 4 != 0 { out.append(0) }
    }

    private static func appendBE(_ v: UInt32, to out: inout [UInt8]) {
        out.append(UInt8((v >> 24) & 0xff))
        out.append(UInt8((v >> 16) & 0xff))
        out.append(UInt8((v >> 8) & 0xff))
        out.append(UInt8(v & 0xff))
    }
}
