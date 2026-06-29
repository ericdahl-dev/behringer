// Piecewise-linear fader conversions — port of fader_db_to_float / fader_float_to_db
// from cli/toast_logic.c. No Apple framework imports; testable on Linux.

/// Convert a bus fader level in dBFS to the mixer's 0.0–1.0 float parameter.
public func faderDBToFloat(_ dB: Float) -> Float {
    if dB >= 10.0  { return 1.0 }
    if dB >= -10.0 { return (dB + 30.0) / 40.0 }
    if dB >= -30.0 { return (dB + 50.0) / 80.0 }
    if dB >= -60.0 { return (dB + 70.0) / 160.0 }
    if dB >= -90.0 { return (dB + 90.0) / 480.0 }
    return 0.0
}

/// Convert a mixer's 0.0–1.0 fader float back to dBFS.
public func faderFloatToDB(_ f: Float) -> Float {
    if f >= 1.0    { return 10.0 }
    if f >= 0.5    { return 40.0  * f - 30.0 }
    if f >= 0.25   { return 80.0  * f - 50.0 }
    if f >= 0.0625 { return 160.0 * f - 70.0 }
    if f >= 0.0    { return 480.0 * f - 90.0 }
    return -90.0
}
