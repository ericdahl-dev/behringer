import XCTest
@testable import ToastSaverCore

final class GEQMapTests: XCTestCase {
    func testCalibratedBinMapsToItsPar() {
        // bin 56 = calibrated 1 kHz point = binForPar[17] → par 18.
        XCTAssertEqual(GEQMap.parForBin(56), 18)
    }

    func testBinSnapsToNearestPar() {
        // bin 1 is closest to binForPar[0]=0 → par 1; bin 5 closest to [1]=3 → par 2.
        XCTAssertEqual(GEQMap.parForBin(1), 1)
        XCTAssertEqual(GEQMap.parForBin(5), 2)
    }

    func testDBEncodesToGEQFloat() {
        XCTAssertEqual(GEQMap.geqFloat(forDB: 0), 0.5, accuracy: 1e-6)
        XCTAssertEqual(GEQMap.geqFloat(forDB: -15), 0.0, accuracy: 1e-6)
        XCTAssertEqual(GEQMap.geqFloat(forDB: 15), 1.0, accuracy: 1e-6)
    }
}
