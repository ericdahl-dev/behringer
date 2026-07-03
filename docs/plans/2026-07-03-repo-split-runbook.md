# Repo split runbook — carving the monorepo into products

**Date:** 2026-07-03
**Status:** plan for review — nothing executed yet
**Decision locked:** shared OSC-protocol docs become their own repo, pulled into
consumers as a **git submodule**.

## Why

`cli/`, `esp32/`, and `ios/` are three independent products with **zero code
cross-references**, each with its own Ordna tracker (`T` / `ESP`+`LNK` / `IOS`),
its own `AGENTS.md`, and its own CI job. The only shared surface is the OSC
protocol reference set. Splitting buys: independent CI (today the Swift
`ToastSaverCore` job runs on every firmware PR), independent versioning/releases,
smaller clones (esp32 alone drags 8.4 MB of vendored SDKs + build artifacts), and
no cross-domain `.gitignore`/build-artifact churn.

This is **isolation-driven, not "finished"-driven** — all three areas are still
active; that's fine, the boundaries are already clean.

## Target topology

| New repo | Contents | Submodule? |
|---|---|---|
| `xair-osc-docs` | the 4 OSC protocol references (below) | — (it IS the submodule) |
| `xair-cli` | `cli/` (XAir/XR18/ToastSaver CLI, X32lib, ring-out/RTA/mic-cal/TUI) | consumes `xair-osc-docs` at `docs/osc/` |
| `x32link-firmware` | `esp32/` (X32Link, X32MidiClock, X32FaderDisp, X32_emulator, vendored `lib/`) | consumes `xair-osc-docs` at `docs/osc/` |
| `toastsaver-ios` | `ios/` (ToastSaver app + ToastSaverCore SPM package) | none (no OSC-doc dependency) |

The current `behringer` repo can either be **renamed to `x32link-firmware`** (it
holds the most history) or kept as an archive. Recommend rename — least history churn.

## Docs categorization (decides what moves where)

**Shared → `xair-osc-docs` (submodule):** pure OSC address/protocol refs, used by
both CLI (control) and firmware (OSC-out):
- `docs/x32-osc-protocol.md`
- `docs/xr18-geq-osc.md`
- `docs/xr18-meters-osc.md`
- `docs/xr18-xair-osc-cheatsheet.md`

**Move WITH `xair-cli`:** `docs/ringout-design.md`, `docs/system-tune-design.md`,
`docs/handoff-toastsaver-cli-tui.md`, `docs/adr/0001-toast-step-event-stream.md`,
`docs/adr/0002-tui-tty-fallback.md`.

**Move WITH `x32link-firmware`:** `docs/adr/0003-firmware-pure-c-glue-split.md`,
`docs/plans/*link*`, `docs/plans/*lnk*`, this runbook.

**Cross-cutting (pick a home, don't submodule):** `docs/ideas.md`,
`docs/agents/*` (domain/issue-tracker/triage), `docs/handoff-*`. Default: keep in
the firmware repo (the de-facto home) and copy the process docs into each area's
`AGENTS.md` where needed.

## Open couplings to resolve BEFORE carving

1. **`X32_emulator` ↔ CLI tests.** `cli/X32lib/X32Connect.c` references
   `esp32/X32_emulator/X32.c`; the CLI integration tests launch the emulator. A
   clean `cli`/`esp32` split breaks that. **DECIDED — option (a):** the emulator
   lives in `x32link-firmware`; `xair-cli` CI clones+builds it as a CI step (not a
   submodule, to avoid coupling the CLI's checkout to firmware revisions). The CLI
   test harness gets an `X32_EMULATOR_BIN` env var pointing at the built binary;
   locally, a `make emulator` target clones/builds it on demand.
   - Rejected: (b) a standalone `x32-emulator` repo submodule'd by both — extra
     repo to maintain for one test dependency; (c) tests targeting a pre-running
     emulator only — loses hermetic CI.
2. **Ordna trackers** are already per-area — no merge needed; each area's `tasks/`
   travels with it. The root `T-*` tracker goes with `xair-cli`.
3. **`build_opt.h`** (esp32) stays empty-at-HEAD per AGENTS.md — verify the split
   preserves that (it will; it's a tracked file with local-only edits).

## Procedure (sequenced)

**Phase 0 — prerequisites**
- Land/merge open PRs first so history is complete: **#40** (LNK-026 + LNK-028),
  **#41** (LNK-027, retarget to master post-#40), **chore/t-038-done**. Do not
  carve mid-flight.
- Install the tool: `brew install git-filter-repo`.
- Resolve the emulator coupling decision (above).

**Phase 1 — carve the shared docs repo (must exist first)**
```bash
git clone https://github.com/ericdahl-dev/behringer.git xair-osc-docs && cd xair-osc-docs
git filter-repo \
  --path docs/x32-osc-protocol.md \
  --path docs/xr18-geq-osc.md \
  --path docs/xr18-meters-osc.md \
  --path docs/xr18-xair-osc-cheatsheet.md \
  --path-rename docs/:            # root the files at repo top
# create the GitHub repo, then:
git remote add origin git@github.com:ericdahl-dev/xair-osc-docs.git
git push -u origin master
```

**Phase 2 — carve each product (history preserved per path)**
```bash
# firmware
git clone https://github.com/ericdahl-dev/behringer.git x32link-firmware && cd x32link-firmware
git filter-repo --path esp32/ --path docs/adr/0003-firmware-pure-c-glue-split.md \
                --path-rename esp32/:            # esp32/* -> repo root
git submodule add git@github.com:ericdahl-dev/xair-osc-docs.git docs/osc
git remote add origin git@github.com:ericdahl-dev/x32link-firmware.git && git push -u origin master

# cli  (repeat pattern with --path cli/ + its docs/adr + ring-out/system-tune docs)
# ios  (--path ios/ ; NO submodule — no OSC-doc dependency)
```

**Phase 3 — wire consumers**
- Replace in-repo OSC-doc references with the submodule path (`docs/osc/…`).
- Emulator coupling (option a): give `xair-cli` a `make emulator` target that
  clones `x32link-firmware` and builds `X32_emulator`, exporting `X32_EMULATOR_BIN`
  for the integration tests.
- Add a per-repo `.gitignore` (drop the cross-domain entries — Xcode `build/`,
  `xcuserdata` only in ios; Arduino build artifacts only in firmware).

**Phase 4 — CI per repo**
- `xair-cli`: `make -C cli` + host tests, with a CI step that clones+builds the
  firmware `X32_emulator` and sets `X32_EMULATOR_BIN` for the integration tests.
- `x32link-firmware`: `cd test && make` (the host suite you already run) + an
  `arduino-cli compile` smoke build.
- `toastsaver-ios`: `xcodebuild`/`swift test` for ToastSaverCore.
- Drop the now-irrelevant jobs from each (no more Swift job on firmware PRs).

**Phase 5 — decommission**
- In the old monorepo: either archive it, or `git filter-repo` it down to just the
  firmware and rename to `x32link-firmware` (skip the fresh clone in Phase 2 if so).
- Update any external links / clones. Leave a `README` pointer in the archived repo.

## Safety / rollback

- All carving happens on **fresh clones**; the monorepo is untouched until Phase 5.
  Nothing is irreversible until you push new remotes and archive the original.
- `git filter-repo` refuses to run on a repo with a remote by default (fresh-clone
  guard) — good; keep it.
- Tag the monorepo (`pre-split-2026-07-03`) before Phase 5 so the unified history
  is always recoverable.

## Not doing (explicitly)

- No history-flattening imports — every carve keeps per-path history/blame.
- No submodule for area-specific docs — only the 4 OSC refs are shared.
- No split of `esp32/lib/` vendored SDKs into submodules (they're pinned vendored
  copies by design; leave them in the firmware repo).
