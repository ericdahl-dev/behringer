#!/usr/bin/env bash
# Doc-staleness guard: fail if AGENTS.md names a source file that doesn't exist.
# This is what would have caught the stale `link_bridge.cpp` reference. It only
# checks file-shaped tokens (*.c/.cpp/.h/.ino, foo.{h,cpp}, foo.*) in backticks
# — commands, OSC paths, and function refs are ignored.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 2   # -> esp32-link/
DOC="AGENTS.md"
DIR="X32Link"

# NOTE: the token scan runs as a top-level `for` loop, NOT `miss=$(… case … )`.
# A `case`'s `)` pattern terminators nested inside `$(…)` make bash miscount the
# command substitution's closing paren ("syntax error near `;;`"), so the whole
# script failed to parse. Keeping the `case` out of any `$(…)` avoids that.
miss=""
# accept either a bare name (resolved under $DIR) or a repo-relative path token
# like `X32Link/build_opt.h` (resolved from cwd).
have() { [ -e "$DIR/$1" ] || [ -e "$1" ]; }
for tok in $(grep -oE '`[^`]+`' "$DOC" | tr -d '`'); do
  f="${tok%%:*}"        # strip :function suffix
  f="${f%%(*}"          # strip (args)
  case "$f" in
    *.c|*.cpp|*.h|*.ino)
      have "$f" || miss="$miss $f" ;;
    *.\{*\})
      base="${f%.\{*}"; exts="${f#*.\{}"; exts="${exts%\}}"
      oldifs=$IFS; IFS=','
      for e in $exts; do have "$base.$e" || miss="$miss $base.$e"; done
      IFS=$oldifs ;;
    *.\*)
      base="${f%.\*}"
      ls "$DIR/$base."* >/dev/null 2>&1 || ls "$base."* >/dev/null 2>&1 || miss="$miss $f" ;;
  esac
done

if [ -n "$miss" ]; then
  echo "FAIL: AGENTS.md references source files that do not exist:"
  printf '  %s\n' $miss
  exit 1
fi
echo "doc check: every source file named in AGENTS.md exists"
