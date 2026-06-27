#!/usr/bin/env python3
import os
import subprocess
import sys


ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(ROOT, "build")
SMOKE = os.path.join(BUILD, ".toolchain-smoke")
SRC = os.path.join(SMOKE, "smoke.c")
BIN = os.path.join(SMOKE, "smoke")


def run(name, command):
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if result.returncode == 0:
        print(f"PASS  {name}")
        return 0
    output = (result.stdout + result.stderr).strip()
    print(f"FAIL  {name}: {output}")
    return 1


def main():
    os.makedirs(SMOKE, exist_ok=True)
    with open(SRC, "w", encoding="utf-8") as handle:
        handle.write(
            "#include <math.h>\n"
            "#include <stdio.h>\n"
            "#include <curses.h>\n"
            "int main(void) {\n"
            "  printf(\"%.0f %d\\n\", sqrt(4.0), OK);\n"
            "  return 0;\n"
            "}\n"
        )

    failed = 0
    failed += run("gcc links libc/libm/ncursesw", ["gcc", SRC, "-o", BIN, "-lm", "-lncursesw"])
    if failed == 0:
        failed += run("toolchain smoke binary runs", [BIN])
    return failed


if __name__ == "__main__":
    sys.exit(main())
