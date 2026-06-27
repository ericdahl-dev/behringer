#!/usr/bin/env bash
# Doc-staleness guard: fail if AGENTS.md names a source file that doesn't exist.
# This is what would have caught the stale `link_bridge.cpp` reference. It only
# checks file-shaped tokens (*.c/.cpp/.h/.ino, foo.{h,cpp}, foo.*) in backticks
# — commands, OSC paths, and function refs are ignored.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 2   # -> esp32-link/
DOC="AGENTS.md"
DIR="X32Link"

miss=$(
  grep -oE '`[^`]+`' "$DOC" | tr -d '`' | while read -r tok; do
    f="${tok%%:*}"        # strip :function suffix
    f="${f%%(*}"          # strip (args)
    case "$f" in
      *.c|*.cpp|*.h|*.ino)
        [ -e "$DIR/$f" ] || echo "$f" ;;
      *.\{*\})
        base="${f%.\{*}"; exts="${f#*.\{}"; exts="${exts%\}}"
        IFS=','; for e in $exts; do [ -e "$DIR/$base.$e" ] || echo "$base.$e"; done ;;
      *.\*)
        base="${f%.\*}"; ls "$DIR/$base."* >/dev/null 2>&1 || echo "$f" ;;
    esac
  done
)

if [ -n "$miss" ]; then
  echo "FAIL: AGENTS.md references source files that do not exist:"
  echo "$miss" | sed 's/^/  /'
  exit 1
fi
echo "doc check: every source file named in AGENTS.md exists"
