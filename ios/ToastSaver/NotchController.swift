import Foundation
import Combine
import ToastSaverCore

struct BandState {
    var active: Bool = false
    var cutValue: Float = 0.5       // OSC float (0–1) sent when notched
    var notchedAt: Date = .distantPast
    var releaseHold: Int = 0        // consecutive quiet frames since last detection
}

final class NotchController: ObservableObject {
    /// parActive[i] = true if GEQ par i+1 is currently notched. Published on main thread.
    @Published private(set) var parActive: [Bool] = Array(repeating: false, count: 31)

    private var bands: [BandState] = Array(repeating: BandState(), count: 31)
    private let connection: XR18Connection
    private let fxSlot: Int
    private let cutDb: Float
    private let releaseSeconds: Float
    private let updateRate: Float = 20.0    // approximate FFT frames/second

    init(connection: XR18Connection, fxSlot: Int = 4, cutDb: Float = -6, releaseSeconds: Float = 10) {
        self.connection = connection
        self.fxSlot = fxSlot
        self.cutDb = cutDb
        self.releaseSeconds = releaseSeconds
    }

    /// Call once per FFT frame with the detected GEQ par (1–31), or nil if no detection.
    /// Must be called off the main thread (FFT dispatch queue).
    func update(detectedPar: Int?) {
        let releaseFrames = Int(releaseSeconds * updateRate)
        let cutValue = GEQMap.geqFloat(forDB: cutDb)

        // Pass 1: advance release ramp for all active bands
        for j in 0..<31 {
            guard bands[j].active else { continue }
            let par = j + 1
            let isCurrentTarget = detectedPar == par

            if !isCurrentTarget {
                bands[j].releaseHold += 1
                let t = min(Float(bands[j].releaseHold) / Float(releaseFrames), 1.0)
                let rampValue = bands[j].cutValue + (0.5 - bands[j].cutValue) * t
                connection.sendTEQBand(slot: fxSlot, par: par, value: rampValue)

                if bands[j].releaseHold >= releaseFrames {
                    bands[j].active = false
                    bands[j].releaseHold = 0
                }
            } else {
                if bands[j].releaseHold > 0 {
                    // Re-triggered mid-fade — snap back to cut depth
                    bands[j].releaseHold = 0
                    connection.sendTEQBand(slot: fxSlot, par: par, value: bands[j].cutValue)
                }
            }
        }

        // Pass 2: apply new notch if detected and not already active
        if let par = detectedPar {
            let j = par - 1
            if !bands[j].active {
                connection.sendTEQBand(slot: fxSlot, par: par, value: cutValue)
                bands[j] = BandState(active: true, cutValue: cutValue, notchedAt: Date(), releaseHold: 0)
            }
        }

        let active = bands.map(\.active)
        DispatchQueue.main.async { self.parActive = active }
    }

    /// Restore all active notches to flat (0.5). Call on disconnect or app resign.
    func restoreAll() {
        for j in 0..<31 where bands[j].active {
            connection.sendTEQBand(slot: fxSlot, par: j + 1, value: 0.5)
            bands[j].active = false
        }
        DispatchQueue.main.async { self.parActive = Array(repeating: false, count: 31) }
    }
}
