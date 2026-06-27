import XCTest
@testable import ToastSaverCore

final class BaselineTrackerTests: XCTestCase {
    func testHalfAlphaMovesHalfwayToTheSignal() {
        var t = BaselineTracker(binCount: 4, initial: 0)
        t.update(with: [10, 10, 10, 10], alpha: 0.5)
        XCTAssertEqual(t.baseline, [5, 5, 5, 5])
        t.update(with: [10, 10, 10, 10], alpha: 0.5)   // 5 → 7.5
        XCTAssertEqual(t.baseline, [7.5, 7.5, 7.5, 7.5])
    }

    func testAlphaOneSnapsToTheSignal() {
        var t = BaselineTracker(binCount: 3, initial: -40)
        t.update(with: [-12, -13, -14], alpha: 1.0)
        XCTAssertEqual(t.baseline, [-12, -13, -14])
    }

    func testAlphaZeroLeavesBaselineUnchanged() {
        var t = BaselineTracker(binCount: 2, initial: -40)
        t.update(with: [0, 0], alpha: 0.0)
        XCTAssertEqual(t.baseline, [-40, -40])
    }
}
