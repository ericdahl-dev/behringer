# Domain Docs

How the engineering skills should consume this repo's domain documentation when
exploring the codebase.

## Layout

This repo uses a single-context domain-doc layout.

Read these if they exist:

- `CONTEXT.md` at the repo root
- `docs/adr/` for architectural decision records

If these files do not exist, proceed silently. Do not flag their absence or
suggest creating them upfront. The producer skill (`grill-with-docs`) creates
them lazily when terms or decisions actually get resolved.

## Use the glossary's vocabulary

When output names a domain concept in an issue title, refactor proposal,
hypothesis, or test name, use the term as defined in `CONTEXT.md`. Avoid
drifting to synonyms the glossary explicitly avoids.

If the concept is not in the glossary yet, either reconsider whether it belongs
to this project language or note the gap for `grill-with-docs`.

## Flag ADR conflicts

If output contradicts an existing ADR, surface that explicitly rather than
silently overriding it.
