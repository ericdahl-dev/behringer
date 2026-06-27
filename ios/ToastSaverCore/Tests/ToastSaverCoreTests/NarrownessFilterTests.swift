import XCTest
@testable import ToastSaverCore

// Mirrors cli/test_toast_logic.c is_narrow_peak cases. Defaults: skip 2, span 3,
// minDB 10 → compares the peak to bins ±3,±4,±5, skipping the two adjacent.
final class NarrownessFilterTests: XCTestCase {
    private func flat(_ v: Float, _ n: Int = 100) -> [Float] { Array(repeating: v, count: n) }

    func testFlatBroadbandPlateauIsNotNarrow() {
        XCTAssertFalse(NarrownessFilter.isNarrowPeak(bins: flat(-30), peakBin: 50))
    }

    func testPeakWithNoLeftNeighborsIsNotNarrow() {
        var bins = flat(-40); bins[1] = -10        // loud, but at bin 1
        XCTAssertFalse(NarrownessFilter.isNarrowPeak(bins: bins, peakBin: 1))
    }

    func testGentleVocalSlopeIsNotNarrow() {
        var bins = flat(-30)
        bins[50] = -28                              // only 2 dB above neighbors
        XCTAssertFalse(NarrownessFilter.isNarrowPeak(bins: bins, peakBin: 50))
    }

    func testNarrowSpikeIsNarrow() {
        var bins = flat(-30)
        bins[50] = -15                              // 15 dB above neighbor floor
        XCTAssertTrue(NarrownessFilter.isNarrowPeak(bins: bins, peakBin: 50))
    }
}
