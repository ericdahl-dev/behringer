import XCTest
@testable import ToastSaverCore

final class MixerModelTests: XCTestCase {

    // MARK: - XR18 fields

    func testXR18Port() { XCTAssertEqual(MixerModel.xr18.port, 10024) }
    func testXR18MetersPath() { XCTAssertEqual(MixerModel.xr18.metersPath, "/meters/4") }
    func testXR18RtaBinCount() { XCTAssertEqual(MixerModel.xr18.rtaBinCount, 100) }
    func testXR18HandshakePath() { XCTAssertEqual(MixerModel.xr18.handshakePath, "/xinfo") }
    func testXR18BusCount() { XCTAssertEqual(MixerModel.xr18.busCount, 6) }
    func testXR18FxSlots() { XCTAssertEqual(MixerModel.xr18.fxSlots, 4) }
    func testXR18InputChannels() { XCTAssertEqual(MixerModel.xr18.inputChannels, 18) }

    // MARK: - X32 fields

    func testX32Port() { XCTAssertEqual(MixerModel.x32.port, 10023) }
    func testX32MetersPath() { XCTAssertEqual(MixerModel.x32.metersPath, "/meters/15") }
    func testX32RtaBinCount() { XCTAssertEqual(MixerModel.x32.rtaBinCount, 50) }
    func testX32HandshakePath() { XCTAssertEqual(MixerModel.x32.handshakePath, "/info") }
    func testX32BusCount() { XCTAssertEqual(MixerModel.x32.busCount, 16) }
    func testX32FxSlots() { XCTAssertEqual(MixerModel.x32.fxSlots, 8) }
    func testX32InputChannels() { XCTAssertEqual(MixerModel.x32.inputChannels, 32) }

    // MARK: - named()

    func testNamedXR18() { XCTAssertEqual(MixerModel.named("XR18"), .xr18) }
    func testNamedXAir() { XCTAssertEqual(MixerModel.named("xair"), .xr18) }
    func testNamedX32() { XCTAssertEqual(MixerModel.named("X32"), .x32) }
    func testNamedM32() { XCTAssertEqual(MixerModel.named("m32"), .x32) }
    func testNamedUnknown() { XCTAssertNil(MixerModel.named("unknown")) }
    func testNamedEmpty() { XCTAssertNil(MixerModel.named("")) }

    // MARK: - Equatable

    func testSameModelEqual() { XCTAssertEqual(MixerModel.xr18, MixerModel.xr18) }
    func testDifferentModelsNotEqual() { XCTAssertNotEqual(MixerModel.xr18, MixerModel.x32) }
}
