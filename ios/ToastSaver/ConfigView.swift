import SwiftUI

struct ConfigView: View {
    @AppStorage("xr18_ip")      var xr18IP: String = "192.168.0.64"
    @AppStorage("fx_slot")      var fxSlot: Int = 4
    @AppStorage("threshold_db") var thresholdDb: Double = 20
    @AppStorage("cut_db")       var cutDb: Double = -6
    @AppStorage("release_sec")  var releaseSec: Double = 10

    @Environment(\.dismiss) var dismiss

    var body: some View {
        NavigationStack {
            Form {
                Section("XR18") {
                    TextField("IP Address", text: $xr18IP)
                        .keyboardType(.numbersAndPunctuation)
                    Stepper("FX Slot: \(fxSlot)", value: $fxSlot, in: 1...4)
                }
                Section("Detection") {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Threshold: \(Int(thresholdDb)) dB")
                            .font(.subheadline)
                        Slider(value: $thresholdDb, in: 5...40, step: 1)
                    }
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Cut: \(Int(cutDb)) dB")
                            .font(.subheadline)
                        Slider(value: $cutDb, in: -15...(-1), step: 1)
                    }
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Release: \(Int(releaseSec)) s")
                            .font(.subheadline)
                        Slider(value: $releaseSec, in: 2...30, step: 1)
                    }
                }
            }
            .navigationTitle("Settings")
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }
}
