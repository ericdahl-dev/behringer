# Issue tracker: Ordna

Issues and tasks for this repo are tracked with Ordna.

Ordna stores tasks as markdown files in `tasks/*.md` when `.ordna/config.yaml`
uses `storage: file` or `storage: hybrid`. This repo currently uses:

```yaml
storage: file
tasksDir: tasks
schema: ordna
statuses: [todo, doing, done]
idPrefix: T
zeroPaddedIds: 3
```

## Conventions

- One task per file: `tasks/<id>.md`
- Task IDs use the configured prefix and padding, such as `T-001`
- The Kanban status is the task frontmatter `status`
- Triage roles are stored as Ordna tags; see `triage-labels.md`
- Acceptance criteria are markdown checkboxes in the `## Acceptance Criteria`
  section
- Progress belongs in the append-only `## Progress` section

## When a skill says "publish to the issue tracker"

Create an Ordna task with `ordna create`, or create a task file in `tasks/`
that matches the Ordna schema. Prefer the CLI when assigning IDs:

```bash
ordna create "Short task title" -t needs-triage
```

Use the configured statuses from `.ordna/config.yaml`. With the current config,
new work starts as `todo`, active work moves to `doing`, and completed work
moves to `done`.

## When a skill says "fetch the relevant ticket"

Use the Ordna CLI:

```bash
ordna show T-001
```

For file-backed storage, reading `tasks/T-001.md` directly is also acceptable.
If the config ever changes to `storage: namespace`, direct file access will not
work; use `ordna list`, `ordna show`, `ordna create`, and `ordna move`.

## When a skill says "update issue state"

Use Ordna statuses for workflow state and tags for triage roles:

```bash
ordna move T-001 doing
```

When editing a task file directly, update `updated_at` to today's date.
