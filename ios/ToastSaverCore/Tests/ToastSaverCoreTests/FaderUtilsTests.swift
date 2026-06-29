import XCTest
@testable import ToastSaverCore

final class FaderUtilsTests: XCTestCase {

    // MARK: - faderDBToFloat (port of fader_db_to_float from cli/toast_logic.c)

    func testAboveCeilingClampsToOne() {
        XCTAssertEqual(faderDBToFloat(10.0), 1.0, accuracy: 1e-6)
        XCTAssertEqual(faderDBToFloat(20.0), 1.0, accuracy: 1e-6)
    }

    func testZeroDBMapsToExpectedFloat() {
        // segment: dB >= -10 → (dB + 30) / 40 → 30/40 = 0.75
        XCTAssertEqual(faderDBToFloat(0.0), 0.75, accuracy: 1e-6)
    }

    func testMinus10DBIsSegmentBoundary() {
        // (–10 + 30) / 40 = 0.5
        XCTAssertEqual(faderDBToFloat(-10.0), 0.5, accuracy: 1e-6)
    }

    func testMinus30DBIsSegmentBoundary() {
        // (–30 + 50) / 80 = 20/80 = 0.25
        XCTAssertEqual(faderDBToFloat(-30.0), 0.25, accuracy: 1e-6)
    }

    func testMinus60DBIsSegmentBoundary() {
        // (–60 + 70) / 160 = 10/160 = 0.0625
        XCTAssertEqual(faderDBToFloat(-60.0), 0.0625, accuracy: 1e-6)
    }

    func testMinus90DBMapsToZero() {
        // (–90 + 90) / 480 = 0
        XCTAssertEqual(faderDBToFloat(-90.0), 0.0, accuracy: 1e-6)
    }

    func testBelowMinus90ClampsToZero() {
        XCTAssertEqual(faderDBToFloat(-100.0), 0.0, accuracy: 1e-6)
    }

    // MARK: - faderFloatToDB (inverse)

    func testRoundTripAtZeroDB() {
        let f = faderDBToFloat(0.0)
        XCTAssertEqual(faderFloatToDB(f), 0.0, accuracy: 1e-4)
    }

    func testRoundTripAtMinus20DB() {
        let f = faderDBToFloat(-20.0)
        XCTAssertEqual(faderFloatToDB(f), -20.0, accuracy: 1e-4)
    }

    func testRoundTripAtMinus45DB() {
        let f = faderDBToFloat(-45.0)
        XCTAssertEqual(faderFloatToDB(f), -45.0, accuracy: 1e-4)
    }

    func testRoundTripAtMinus75DB() {
        let f = faderDBToFloat(-75.0)
        XCTAssertEqual(faderFloatToDB(f), -75.0, accuracy: 1e-4)
    }

    func testFloatOneMapsToPlusTen() {
        XCTAssertEqual(faderFloatToDB(1.0), 10.0, accuracy: 1e-6)
    }

    func testFloatZeroMapsToMinus90() {
        XCTAssertEqual(faderFloatToDB(0.0), -90.0, accuracy: 1e-6)
    }
}
