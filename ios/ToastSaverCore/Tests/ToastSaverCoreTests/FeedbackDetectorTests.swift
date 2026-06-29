import XCTest
@testable import ToastSaverCore

// MARK: - PeakDetector (stateless)

final class PeakDetectorTests: XCTestCase {
    private func flat(_ v: Float, _ n: Int = 100) -> [Float] { Array(repeating: v, count: n) }

    func testReturnsNilWhenNothingExceedsThreshold() {
        let bins = flat(-40), base = flat(-40)
        XCTAssertNil(PeakDetector.detectPeak(bins: bins, baseline: base, threshold: 6))
    }

    func testDetectsBinThatExceedsBaselinePlusThreshold() {
        var bins = flat(-40); bins[42] = -20   // +20 over baseline
        let peak = PeakDetector.detectPeak(bins: bins, baseline: flat(-40), threshold: 6)
        XCTAssertEqual(peak, 42)
    }

    func testPicksHighestAmplitudeAmongQualifyingBins() {
        var bins = flat(-40); bins[10] = -25; bins[70] = -18  // both exceed, 70 is louder
        let peak = PeakDetector.detectPeak(bins: bins, baseline: flat(-40), threshold: 6)
        XCTAssertEqual(peak, 70)
    }

    func testThresholdIsStrict() {
        var bins = flat(-40); bins[5] = -34    // exactly +6 — must NOT trip (strict >)
        XCTAssertNil(PeakDetector.detectPeak(bins: bins, baseline: flat(-40), threshold: 6))
    }
}

// MARK: - FeedbackDetector (stateful, reactive mode)

final class FeedbackDetectorTests: XCTestCase {
    // 100 RTA bins flat at floor; spike injected at `idx`.
    private func flat(_ v: Float = -80.0) -> [Float] { Array(repeating: v, count: 100) }
    private func spiked(at idx: Int, spike: Float = -10.0) -> [Float] {
        var b = flat(); b[idx] = spike; return b
    }

    // narrowSkip=1, narrowSpan=1: a spike with at least 1 clear neighbour each side qualifies.
    private func makeDetector(confirm: Int = 3) -> FeedbackDetector {
        FeedbackDetector(threshold: 20.0, confirmFrames: confirm,
                         narrowSkip: 1, narrowSpan: 1, narrowMinDB: 5.0)
    }

    func testReturnsNilWhenNoPeak() {
        let det = makeDetector()
        XCTAssertNil(det.process(bins: flat(), baseline: flat()))
    }

    func testDoesNotFireBeforeConfirmFrames() {
        let det = makeDetector(confirm: 3)
        let spike = spiked(at: 56)
        for i in 0..<2 {
            XCTAssertNil(det.process(bins: spike, baseline: flat()), "frame \(i+1) should be nil")
        }
    }

    func testFiresOnConfirmFrame() {
        let det = makeDetector(confirm: 3)
        let spike = spiked(at: 56)
        var result: Int? = nil
        for _ in 0..<3 { result = det.process(bins: spike, baseline: flat()) }
        XCTAssertEqual(result, GEQMap.parForBin(56))
    }

    func testResetsCounterAfterFiring() {
        let det = makeDetector(confirm: 2)
        let spike = spiked(at: 56)
        _ = det.process(bins: spike, baseline: flat())
        _ = det.process(bins: spike, baseline: flat())   // fires, resets
        // One more frame alone must not fire.
        XCTAssertNil(det.process(bins: spike, baseline: flat()))
    }

    func testChangingPeakResetsCount() {
        let det = makeDetector(confirm: 3)
        let spike56 = spiked(at: 56)
        let spike70 = spiked(at: 70)
        _ = det.process(bins: spike56, baseline: flat())
        _ = det.process(bins: spike56, baseline: flat())
        // Switch bin — count should reset to 1.
        XCTAssertNil(det.process(bins: spike70, baseline: flat()))
    }

    func testSuppressesAlreadyNotchedPar() {
        let det = makeDetector(confirm: 1)
        let spike = spiked(at: 56)
        let par = GEQMap.parForBin(56)
        XCTAssertNil(det.process(bins: spike, baseline: flat(), notchedPars: [par]))
    }

    func testQuietFrameResetsCount() {
        let det = makeDetector(confirm: 3)
        let spike = spiked(at: 56)
        _ = det.process(bins: spike, baseline: flat())
        _ = det.process(bins: spike, baseline: flat())
        _ = det.process(bins: flat(),  baseline: flat())  // quiet → reset
        _ = det.process(bins: spike, baseline: flat())
        XCTAssertNil(det.process(bins: spike, baseline: flat()))  // only 2 frames since reset
    }

    func testResetClearsState() {
        let det = makeDetector(confirm: 3)
        let spike = spiked(at: 56)
        // Build up 2 of the 3 required frames, then reset.
        _ = det.process(bins: spike, baseline: flat())
        _ = det.process(bins: spike, baseline: flat())
        det.reset()
        // Two frames after reset — still only 2/3, must not fire.
        _ = det.process(bins: spike, baseline: flat())
        XCTAssertNil(det.process(bins: spike, baseline: flat()))
    }
}
