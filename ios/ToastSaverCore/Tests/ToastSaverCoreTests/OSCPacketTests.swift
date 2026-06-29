import XCTest
@testable import ToastSaverCore

final class OSCPacketTests: XCTestCase {
    func testEncodesAddressTypeTagAndFloatWithPadding() {
        let m = OSCMessage("/x", .float(1.0))
        // "/x\0\0" + ",f\0\0" + 1.0f big-endian (0x3f800000)
        XCTAssertEqual(m.bytes, [
            0x2f, 0x78, 0x00, 0x00,
            0x2c, 0x66, 0x00, 0x00,
            0x3f, 0x80, 0x00, 0x00,
        ])
    }

    func testFourCharAddressGetsAFullNullPadWord() {
        // OSC strings are null-terminated then padded to 4 → "/abc" becomes 8 bytes.
        let m = OSCMessage("/abc")
        XCTAssertEqual(m.bytes.count, 8 + 4)   // address(8) + ","(empty tag, 4)
        XCTAssertEqual(Array(m.bytes.prefix(8)), [0x2f, 0x61, 0x62, 0x63, 0, 0, 0, 0])
    }

    func testEncodesInt32BigEndian() {
        let m = OSCMessage("/n", .int32(42))
        XCTAssertEqual(Array(m.bytes.suffix(4)), [0x00, 0x00, 0x00, 0x2a])
    }

    // /xinfo: 6-char address → 8-byte padded, zero-arg type tag → 4 bytes. Total 12.
    func testXinfoEncodesCorrectly() {
        let m = OSCMessage.xinfo()
        XCTAssertEqual(m.bytes, [
            0x2f, 0x78, 0x69, 0x6e, 0x66, 0x6f, 0x00, 0x00,  // "/xinfo\0\0"
            0x2c, 0x00, 0x00, 0x00,                            // ",\0\0\0"
        ])
    }

    // /xremote: 8-char address → 12-byte padded, zero-arg type tag → 4 bytes. Total 16.
    func testXremoteEncodesCorrectly() {
        let m = OSCMessage.xremote()
        XCTAssertEqual(m.bytes, [
            0x2f, 0x78, 0x72, 0x65, 0x6d, 0x6f, 0x74, 0x65,  // "/xremote"
            0x00, 0x00, 0x00, 0x00,                            // null + 3-byte pad
            0x2c, 0x00, 0x00, 0x00,                            // ",\0\0\0"
        ])
    }

    func testHandshakeUsesModelPath() {
        XCTAssertEqual(OSCMessage.handshake(for: .xr18).bytes, OSCMessage.xinfo().bytes)
        let x32 = OSCMessage.handshake(for: .x32)
        // /info starts with 0x2f 0x69 0x6e 0x66 0x6f ('/' 'i' 'n' 'f' 'o')
        XCTAssertEqual(Array(x32.bytes.prefix(5)), [0x2f, 0x69, 0x6e, 0x66, 0x6f])
    }

    func testGEQBandHelperBuildsAddressAndEncodedCut() {
        let m = OSCMessage.geqBand(slot: 1, par: 18, dB: -6)   // (-6+15)/30 = 0.3
        let addr = "/fx/1/par/18"
        XCTAssertEqual(Array(m.bytes.prefix(addr.utf8.count)), Array(addr.utf8))
        // last 4 bytes = 0.3f big-endian
        let v: Float = 0.3
        let be = v.bitPattern.bigEndian
        XCTAssertEqual(Array(m.bytes.suffix(4)), withUnsafeBytes(of: be) { Array($0) })
    }
}
