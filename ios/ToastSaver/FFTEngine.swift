import Accelerate

final class FFTEngine {
    let fftSize: Int
    let sampleRate: Double
    private(set) var geqBins: [Int]

    private let fftSetup: FFTSetup
    private let log2n: vDSP_Length
    private var window: [Float]

    init(fftSize: Int = 2048, sampleRate: Double = 48000) {
        precondition(fftSize > 0 && (fftSize & (fftSize - 1)) == 0, "fftSize must be a power of 2")
        self.fftSize = fftSize
        self.sampleRate = sampleRate
        let n = vDSP_Length(log2(Double(fftSize)))
        self.log2n = n
        self.fftSetup = vDSP_create_fftsetup(n, FFTRadix(kFFTRadix2))!
        var win = [Float](repeating: 0, count: fftSize)
        vDSP_hann_window(&win, vDSP_Length(fftSize), Int32(vDSP_HANN_NORM))
        self.window = win
        let freqs: [Double] = [20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160,
                                200, 250, 315, 400, 500, 630, 800, 1000, 1250,
                                1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000,
                                10000, 12500, 16000, 20000]
        self.geqBins = freqs.map { hz in
            min(Int(round(hz * Double(fftSize) / sampleRate)), fftSize / 2 - 1)
        }
    }

    deinit {
        vDSP_destroy_fftsetup(fftSetup)
    }

    func process(_ samples: [Float]) -> [Float] {
        precondition(samples.count == fftSize)

        // 1. Apply Hann window
        var windowed = [Float](repeating: 0, count: fftSize)
        vDSP_vmul(samples, 1, window, 1, &windowed, 1, vDSP_Length(fftSize))

        // 2–4. Split, FFT, magnitude² — all within scoped unsafe pointer lifetimes
        var realp = [Float](repeating: 0, count: fftSize / 2)
        var imagp = [Float](repeating: 0, count: fftSize / 2)
        var magnitudes = [Float](repeating: 0, count: fftSize / 2)
        realp.withUnsafeMutableBufferPointer { realBuf in
            imagp.withUnsafeMutableBufferPointer { imagBuf in
                var sc = DSPSplitComplex(realp: realBuf.baseAddress!, imagp: imagBuf.baseAddress!)
                windowed.withUnsafeBytes { ptr in
                    let typedPtr = ptr.bindMemory(to: DSPComplex.self)
                    vDSP_ctoz(typedPtr.baseAddress!, 2, &sc, 1, vDSP_Length(fftSize / 2))
                }
                vDSP_fft_zrip(fftSetup, &sc, 1, log2n, FFTDirection(FFT_FORWARD))
                vDSP_zvmags(&sc, 1, &magnitudes, 1, vDSP_Length(fftSize / 2))
            }
        }

        // 5. Convert to dBFS; scale so full-scale sine → 0 dBFS
        let scale = 1.0 / Float(fftSize * fftSize / 4)
        var scaled = magnitudes.map { $0 * scale }
        var db = [Float](repeating: 0, count: fftSize / 2)
        vDSP_vdbcon(&scaled, 1, [10.0], &db, 1, vDSP_Length(fftSize / 2), 0)
        return db
    }

    /// Map an FFT bin index to the nearest GEQ par (1–31).
    func geqPar(forBin bin: Int) -> Int {
        var bestPar = 1
        var bestDist = Int.max
        for (i, geqBin) in geqBins.enumerated() {
            let dist = abs(geqBin - bin)
            if dist < bestDist {
                bestDist = dist
                bestPar = i + 1
            }
        }
        return bestPar
    }
}
