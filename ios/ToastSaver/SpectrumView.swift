import SwiftUI

struct SpectrumView: View {
    let spectrum: [Float]
    let geqBins: [Int]
    let parActive: [Bool]
    let threshold: Float

    var body: some View {
        Canvas { context, size in
            guard !spectrum.isEmpty else { return }
            let binCount = spectrum.count
            let barWidth = size.width / CGFloat(binCount)
            let maxHeight = size.height

            for bin in 0..<binCount {
                let db = max(-80, min(0, spectrum[bin]))
                let heightFraction = (db + 80) / 80
                let barHeight = CGFloat(heightFraction) * maxHeight
                let x = CGFloat(bin) * barWidth
                let rect = CGRect(
                    x: x,
                    y: size.height - barHeight,
                    width: max(barWidth - 0.5, 0.5),
                    height: barHeight
                )

                let color: Color
                var isActive = false
                for j in 0..<min(parActive.count, geqBins.count) {
                    if parActive[j] && abs(bin - geqBins[j]) <= 1 {
                        isActive = true
                        break
                    }
                }

                if isActive {
                    color = .red
                } else if spectrum[bin] > threshold {
                    color = .yellow
                } else {
                    color = Color.green.opacity(0.7)
                }

                context.fill(Path(rect), with: .color(color))
            }
        }
        .background(Color.black)
    }
}
