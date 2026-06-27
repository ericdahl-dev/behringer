import XCTest
@testable import ToastSaverCore

final class FeedbackDetectorTests: XCTestCase {
    private func flat(_ v: Float, _ n: Int = 100) -> [Float] { Array(repeating: v, count: n) }

    func testReturnsNilWhenNothingExceedsThreshold() {
        let bins = flat(-40), base = flat(-40)
        XCTAssertNil(FeedbackDetector.detectPeak(bins: bins, baseline: base, threshold: 6))
    }

    func testDetectsBinThatExceedsBaselinePlusThreshold() {
        var bins = flat(-40); bins[42] = -20   // +20 over baseline
        let peak = FeedbackDetector.detectPeak(bins: bins, baseline: flat(-40), threshold: 6)
        XCTAssertEqual(peak, 42)
    }

    func testPicksHighestAmplitudeAmongQualifyingBins() {
        var bins = flat(-40); bins[10] = -25; bins[70] = -18  // both exceed, 70 is louder
        let peak = FeedbackDetector.detectPeak(bins: bins, baseline: flat(-40), threshold: 6)
        XCTAssertEqual(peak, 70)
    }

    func testThresholdIsStrict() {
        var bins = flat(-40); bins[5] = -34    // exactly +6, must NOT trip (strict >)
        XCTAssertNil(FeedbackDetector.detectPeak(bins: bins, baseline: flat(-40), threshold: 6))
    }
}
