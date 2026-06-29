import Accelerate

final class FFTEngine {
    let fftSize: Int
    let sampleRate: Double

    init(fftSize: Int = 2048, sampleRate: Double = 48000) {
        self.fftSize = fftSize
        self.sampleRate = sampleRate
    }
}
