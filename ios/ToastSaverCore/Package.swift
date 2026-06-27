// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "ToastSaverCore",
    platforms: [.iOS(.v16), .macOS(.v13)],
    products: [
        .library(name: "ToastSaverCore", targets: ["ToastSaverCore"]),
    ],
    targets: [
        .target(name: "ToastSaverCore", dependencies: []),
        .testTarget(name: "ToastSaverCoreTests", dependencies: ["ToastSaverCore"]),
    ]
)
