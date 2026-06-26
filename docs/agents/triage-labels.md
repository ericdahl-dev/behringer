# Triage Labels

The skills speak in terms of five canonical triage roles. This file maps those
roles to the actual Ordna tag strings used in this repo.

| Label in mattpocock/skills | Ordna tag | Meaning                                  |
| -------------------------- | --------- | ---------------------------------------- |
| `needs-triage`             | `needs-triage` | Maintainer needs to evaluate this issue  |
| `needs-info`               | `needs-info` | Waiting on reporter for more information |
| `ready-for-agent`          | `ready-for-agent` | Fully specified, ready for an AFK agent  |
| `ready-for-human`          | `ready-for-human` | Requires human implementation            |
| `wontfix`                  | `wontfix` | Will not be actioned                     |

When a skill mentions a role, use the corresponding Ordna tag from this table.

Ordna task workflow status remains separate from triage tags. Use `todo`,
`doing`, and `done` for board movement unless `.ordna/config.yaml` changes.
