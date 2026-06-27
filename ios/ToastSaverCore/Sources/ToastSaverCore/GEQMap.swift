// Maps RTA bins to 31-band GEQ parameters and encodes dB cuts to the mixer's
// 0…1 float range. Ported from cli/toast_logic.c (bin_to_geq_par, db_to_geq_float).

public enum GEQMap {
    /// RTA bin index for each GEQ par (index 0 = par 01).
    /// Calibrated: pistonphone 1 kHz @ 94 dB SPL → bin 56.
    public static let binForPar: [Int] = [
         0,  3,  7, 10, 13, 16, 20, 23, 26, 30,
        33, 36, 39, 43, 46, 49, 53, 56, 59, 63,
        66, 69, 72, 76, 79, 82, 86, 89, 92, 96,
        99,
    ]

    /// Map an RTA bin index (0–99) to the nearest GEQ par number (1–31).
    public static func parForBin(_ bin: Int) -> Int {
        var best = 0
        var bestDist = Int.max
        for i in 0..<binForPar.count {
            let d = abs(binForPar[i] - bin)
            if d < bestDist { bestDist = d; best = i }
        }
        return best + 1
    }

    /// Encode a dB value to a GEQ float in [0, 1].
    /// 0 dB → 0.5, −15 dB → 0.0, +15 dB → 1.0.
    public static func geqFloat(forDB dB: Float) -> Float {
        (dB + 15.0) / 30.0
    }
}
