#!/usr/bin/env python3
import os
import subprocess
import sys


ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(ROOT, "build")
DEFAULT_TARGETS = ["X32", "XAir", "X32Tap", "test_x32tap"]
SMOKE_CHECKS = {
    "test_x32tap": ([], "16 passed, 0 failed"),
    "X32Tap": (["-h"], "usage: X32Tap"),
    "X32Wav_Xlive": (["-h"], "usage: X32Wav_Xlive"),
    "X32Xlive_Wav": (["-h"], "usage: X32Xlive_wav"),
    "XAir_ToastSaver": (["-h"], "usage: XAir_ToastSaver"),
    "test_toast_logic": ([], "25 passed, 0 failed"),
}


def ok(name):
    print(f"PASS  {name}")


def fail(name, reason):
    print(f"FAIL  {name}: {reason}")


def check_binary(name):
    path = os.path.join(BUILD, name)
    if not os.path.exists(path):
        return False, "missing artifact"
    if not os.access(path, os.X_OK):
        return False, "not executable"
    return True, ""


def check_build(name):
    result = subprocess.run(
        ["make", name],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        output = (result.stdout + result.stderr).strip()
        if "No rule to make target" in output:
            return False, "target missing"
        return False, f"compile/link failed: {output}"
    return True, ""


def check_run(binary, args, expected):
    path = os.path.join(BUILD, binary)
    if not os.path.exists(path):
        return False, "missing artifact"
    result = subprocess.run([path, *args], cwd=ROOT, text=True, capture_output=True)
    output = result.stdout + result.stderr
    if result.returncode != 0:
        return False, f"exit {result.returncode}: {output.strip()}"
    if expected not in output:
        return False, f"missing output {expected!r}"
    return True, ""


def selected_targets():
    raw = os.environ.get("BUILD_CHECK_TARGETS", "")
    if not raw.strip():
        return DEFAULT_TARGETS
    return [target for target in raw.replace(",", " ").split() if target]


def main():
    passed = 0
    failed = 0

    for target in selected_targets():
        name = f"build target: {target}"
        success, reason = check_build(target)

        if success:
            passed += 1
            ok(name)
        else:
            failed += 1
            fail(name, reason)
            continue

        name = f"binary exists: {target}"
        success, reason = check_binary(target)

        if success:
            passed += 1
            ok(name)
        else:
            failed += 1
            fail(name, reason)
            continue

        if target in SMOKE_CHECKS:
            name = f"{target} passes"
            args, expected = SMOKE_CHECKS[target]
            success, reason = check_run(target, args, expected)

            if success:
                passed += 1
                ok(name)
            else:
                failed += 1
                fail(name, reason)

    print(f"\n{passed} passed, {failed} failed")
    return failed


if __name__ == "__main__":
    sys.exit(main())
