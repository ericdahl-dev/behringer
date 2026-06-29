import XCTest
@testable import ToastSaverCore

// Helpers shared across tests
private func flatLevels(_ value: Float = -60.0) -> [Float] {
    [Float](repeating: value, count: 31)
}

// Inject a narrow spike on GEQ index `idx` (par = idx+1) into a flat background.
private func spikedLevels(at idx: Int, spike: Float = -10.0, floor: Float = -60.0) -> [Float] {
    var bins = flatLevels(floor)
    bins[idx] = spike
    return bins
}

// Make a minimal config with known, deterministic values.
private func makeConfig(
    start: Float = -20.0, ceiling: Float = 0.0, step: Float = 3.0,
    margin: Float = 6.0, maxNotches: Int = 4,
    threshold: Float = 20.0, cut: Float = -6.0,
    confirm: Int = 3, settle: Int = 2, stable: Int = 2,
    narrowSkip: Int = 1, narrowSpan: Int = 1, narrowMinDB: Float = 5.0
) -> RingoutConfig {
    RingoutConfig(
        startGainDB: start, ceilingDB: ceiling, stepDB: step,
        targetMarginDB: margin, maxNotches: maxNotches,
        thresholdDB: threshold, cutDB: cut,
        confirmFrames: confirm, settleFrames: settle, stableFrames: stable,
        narrowSkip: narrowSkip, narrowSpan: narrowSpan, narrowMinDB: narrowMinDB
    )
}

// Drive `controller` with `stableFrames` quiet frames to reach "raise" readiness.
private func driveToStable(_ controller: RingoutController) {
    let quiet = flatLevels()
    let baseline = flatLevels()
    for _ in 0..<controller.config.stableFrames {
        _ = controller.step(geqLevels: quiet, baseline: baseline)
    }
}

final class RingoutControllerTests: XCTestCase {

    // MARK: - Basic raise cycle

    func testRaisesAfterStableFrames() {
        let cfg = makeConfig()
        let ctrl = RingoutController(config: cfg)
        let quiet = flatLevels()
        let baseline = flatLevels()

        var action: RingoutAction = .hold
        for _ in 0..<cfg.stableFrames {
            action = ctrl.step(geqLevels: quiet, baseline: baseline)
        }
        // After stableFrames quiet frames, should raise.
        XCTAssertEqual(action, .raiseGain(to: cfg.startGainDB + cfg.stepDB))
        XCTAssertEqual(ctrl.currentGainDB, cfg.startGainDB + cfg.stepDB, accuracy: 1e-4)
    }

    func testHoldsWhileStillAccumulatingStableFrames() {
        let cfg = makeConfig(stable: 5)
        let ctrl = RingoutController(config: cfg)
        let quiet = flatLevels()
        let baseline = flatLevels()

        for i in 0..<4 {
            let action = ctrl.step(geqLevels: quiet, baseline: baseline)
            XCTAssertEqual(action, .hold, "Frame \(i) should still hold")
        }
    }

    func testSettleBlocksAnalysisAfterRaise() {
        let cfg = makeConfig(settle: 3, stable: 2)
        let ctrl = RingoutController(config: cfg)
        let quiet = flatLevels()
        let baseline = flatLevels()

        // Reach a raise (stableFrames=2 means two quiet frames).
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)   // stable frame 1
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)   // stable frame 2 → raise
        XCTAssertEqual(ctrl.step(geqLevels: quiet, baseline: baseline), .hold) // settle 1
        XCTAssertEqual(ctrl.step(geqLevels: quiet, baseline: baseline), .hold) // settle 2
        XCTAssertEqual(ctrl.step(geqLevels: quiet, baseline: baseline), .hold) // settle 3
        // After settle, need stableFrames=2 quiet frames before next raise.
        let action = ctrl.step(geqLevels: quiet, baseline: baseline)
        XCTAssertEqual(action, .hold) // stable frame 1 of 2 — not yet raised
    }

    // MARK: - Notch cycle

    func testPlacesNotchAfterConfirmFrames() {
        let cfg = makeConfig(confirm: 3, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let baseline = flatLevels(-80.0)
        let quiet = flatLevels(-80.0)

        // Drive to first raise.
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise

        // Inject a narrow spike on par 10 (index 9).
        // With narrowSkip=1, narrowSpan=1: neighbours are at ±2, which exist.
        let spike = spikedLevels(at: 9, spike: -10.0, floor: -80.0)

        var action: RingoutAction = .hold
        for _ in 0..<3 {
            action = ctrl.step(geqLevels: spike, baseline: baseline)
        }
        XCTAssertEqual(action, .placeNotch(par: 10, cutDB: cfg.cutDB))
        XCTAssertEqual(ctrl.notchCount, 1)
    }

    func testDoesNotNotchBeforeConfirmFrames() {
        let cfg = makeConfig(confirm: 3, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let baseline = flatLevels(-80.0)
        let quiet = flatLevels(-80.0)
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise

        let spike = spikedLevels(at: 9, spike: -10.0, floor: -80.0)
        for i in 0..<2 {
            let action = ctrl.step(geqLevels: spike, baseline: baseline)
            XCTAssertEqual(action, .hold, "Should hold on confirm frame \(i+1)")
        }
    }

    func testBackOffAfterNotch() {
        let cfg = makeConfig(step: 3.0, confirm: 1, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let baseline = flatLevels(-80.0)
        let quiet = flatLevels(-80.0)
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise to start+3

        let gainBeforeNotch = ctrl.currentGainDB
        let spike = spikedLevels(at: 9, spike: -10.0, floor: -80.0)
        _ = ctrl.step(geqLevels: spike, baseline: baseline)  // notch

        let action = ctrl.step(geqLevels: quiet, baseline: baseline)
        XCTAssertEqual(action, .backOff(to: gainBeforeNotch - cfg.stepDB), accuracy: 1e-4)
    }

    func testConfirmResetsOnDifferentBin() {
        let cfg = makeConfig(confirm: 3, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let baseline = flatLevels(-80.0)
        let quiet = flatLevels(-80.0)
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise

        // Two frames on bin 9, then switch to bin 14 — should not notch bin 9.
        let spike9 = spikedLevels(at: 9,  spike: -10.0, floor: -80.0)
        let spike14 = spikedLevels(at: 14, spike: -10.0, floor: -80.0)
        _ = ctrl.step(geqLevels: spike9,  baseline: baseline)
        _ = ctrl.step(geqLevels: spike9,  baseline: baseline)
        let action = ctrl.step(geqLevels: spike14, baseline: baseline)
        XCTAssertEqual(action, .hold)  // reset, now on frame 1 of 3 for par 15
    }

    // MARK: - Stop conditions

    func testMaxNotchesReturnsDone() {
        let cfg = makeConfig(maxNotches: 2, confirm: 1, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let baseline = flatLevels(-80.0)
        let quiet = flatLevels(-80.0)

        func raiseAndNotch(at idx: Int) {
            _ = ctrl.step(geqLevels: quiet, baseline: baseline)       // raise
            let spike = spikedLevels(at: idx, spike: -10.0, floor: -80.0)
            _ = ctrl.step(geqLevels: spike, baseline: baseline)       // notch
            _ = ctrl.step(geqLevels: quiet, baseline: baseline)       // backoff
            // settle (0 frames), then stable frame
        }

        raiseAndNotch(at: 5)
        raiseAndNotch(at: 10)

        // Now at maxNotches; next stable frame should return .done
        let action = ctrl.step(geqLevels: quiet, baseline: baseline)
        if case .done(let reason, _) = action {
            XCTAssertEqual(reason, .maxNotchesReached)
        } else {
            XCTFail("Expected .done(maxNotchesReached), got \(action)")
        }
    }

    func testTargetMarginReturnsDone() {
        // margin=3, step=3, start=-20, ceiling=0 → one raise hits the margin
        let cfg = makeConfig(start: -20, ceiling: 0, step: 3, margin: 3,
                             maxNotches: 8, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let quiet = flatLevels(-80.0)
        let baseline = flatLevels(-80.0)

        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise to -17
        // settle (0 frames); now stable
        let action = ctrl.step(geqLevels: quiet, baseline: baseline)  // stableFrame 1 → done
        if case .done(let reason, let margin) = action {
            XCTAssertEqual(reason, .targetMarginReached)
            XCTAssertEqual(margin, 3.0, accuracy: 1e-4)
        } else {
            XCTFail("Expected .done(targetMarginReached), got \(action)")
        }
    }

    func testCeilingReturnsDone() {
        // start=-3, ceiling=0, step=3, margin=0 (disabled) → raise would exceed ceiling
        let cfg = makeConfig(start: -3, ceiling: 0, step: 3, margin: 0,
                             maxNotches: 8, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let quiet = flatLevels(-80.0)
        let baseline = flatLevels(-80.0)

        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise to 0 (= ceiling)
        // next stable pass: next step (0+3=3) > ceiling(0) → done
        let action = ctrl.step(geqLevels: quiet, baseline: baseline)
        if case .done(let reason, _) = action {
            XCTAssertEqual(reason, .ceilingReached)
        } else {
            XCTFail("Expected .done(ceilingReached), got \(action)")
        }
    }

    func testRunawayAborts() {
        let cfg = makeConfig(confirm: 1, settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let baseline = flatLevels(-80.0)
        let quiet = flatLevels(-80.0)

        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise

        // Place a notch on par 10.
        let spike = spikedLevels(at: 9, spike: -10.0, floor: -80.0)
        _ = ctrl.step(geqLevels: spike, baseline: baseline)  // notch par 10
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // backoff

        // Same par rings again — should abort.
        let action = ctrl.step(geqLevels: spike, baseline: baseline)
        XCTAssertEqual(action, .abort(reason: .runaway))
    }

    // MARK: - Terminal state replay

    func testDoneRepeatsOnSubsequentCalls() {
        let cfg = makeConfig(start: -3, ceiling: 0, step: 3, margin: 0,
                             settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let quiet = flatLevels(-80.0)
        let baseline = flatLevels(-80.0)

        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise to ceiling
        let first = ctrl.step(geqLevels: quiet, baseline: baseline)  // done
        let second = ctrl.step(geqLevels: quiet, baseline: baseline) // should replay done
        XCTAssertEqual(first, second)
    }

    // MARK: - reset()

    func testResetRestoresInitialState() {
        let cfg = makeConfig(confirm: 1, settle: 0, stable: 2)
        let ctrl = RingoutController(config: cfg)
        let baseline = flatLevels(-80.0)
        let quiet = flatLevels(-80.0)

        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // stable 1
        _ = ctrl.step(geqLevels: quiet, baseline: baseline)  // raise
        let spike = spikedLevels(at: 9, spike: -10.0, floor: -80.0)
        _ = ctrl.step(geqLevels: spike, baseline: baseline)  // notch
        XCTAssertEqual(ctrl.notchCount, 1)

        ctrl.reset()

        XCTAssertEqual(ctrl.notchCount, 0)
        XCTAssertEqual(ctrl.currentGainDB, cfg.startGainDB, accuracy: 1e-4)
        // After reset, stableFrames=2 so first quiet frame is still accumulating.
        let action = ctrl.step(geqLevels: quiet, baseline: baseline)
        XCTAssertEqual(action, .hold)
    }

    // MARK: - Gain ceiling clamping

    func testGainNeverExceedsCeiling() {
        let cfg = makeConfig(start: -2, ceiling: 0, step: 3, margin: 0,
                             settle: 0, stable: 1)
        let ctrl = RingoutController(config: cfg)
        let quiet = flatLevels(-80.0)
        let baseline = flatLevels(-80.0)

        for _ in 0..<20 {
            _ = ctrl.step(geqLevels: quiet, baseline: baseline)
            XCTAssertLessThanOrEqual(ctrl.currentGainDB, cfg.ceilingDB + 1e-4)
        }
    }
}

// XCTestCase equality helper for RingoutAction with Float payloads.
private func XCTAssertEqual(_ lhs: RingoutAction, _ rhs: RingoutAction,
                             accuracy: Float = 1e-4,
                             file: StaticString = #file, line: UInt = #line) {
    switch (lhs, rhs) {
    case (.hold, .hold): break
    case (.raiseGain(let a), .raiseGain(let b)):
        XCTAssertEqual(a, b, accuracy: accuracy, file: file, line: line)
    case (.backOff(let a), .backOff(let b)):
        XCTAssertEqual(a, b, accuracy: accuracy, file: file, line: line)
    case (.placeNotch(let pa, let ca), .placeNotch(let pb, let cb)):
        XCTAssertEqual(pa, pb, file: file, line: line)
        XCTAssertEqual(ca, cb, accuracy: accuracy, file: file, line: line)
    case (.done(let ra, let ma), .done(let rb, let mb)):
        XCTAssertEqual(ra, rb, file: file, line: line)
        XCTAssertEqual(ma, mb, accuracy: accuracy, file: file, line: line)
    case (.abort(let ra), .abort(let rb)):
        XCTAssertEqual(ra, rb, file: file, line: line)
    default:
        XCTFail("Expected \(rhs), got \(lhs)", file: file, line: line)
    }
}
