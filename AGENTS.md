# Persistent agent instructions

## Persistent source of truth

This repository is the persistent source of truth. Do not depend on previous
conversations or unpublished context.

At the beginning of every task:

1. Read `AGENTS.md`.
2. Read `TASKS.md`.
3. Inspect the relevant implementation.
4. Inspect the relevant tests.
5. Run `git status --short`.
6. Work only on the first pending task in `TASKS.md`.

One Codex execution processes exactly one task. Never continue to the next
task; the external loop starts a separate execution for it.

## Architecture

Keep the import and backend path source-neutral:

```text
source importer
    |
    v
neutral import model
    |
    v
SqliteModuleWriter
    |
    v
SQLite module format v1
    |
    v
SqliteBibleBackend
    |
    v
BibleBackend
```

- OSIS and USFM are peer importers.
- Never convert OSIS to temporary USFM.
- Importers must not write SQL directly.
- `SqliteModuleWriter` remains source-agnostic.
- `SqliteBibleBackend` remains source-agnostic.
- SWORD is a compatibility backend only; SQLite is the primary backend.

## Invariants

- Preserve SQLite schema v1 and compatible `PRAGMA user_version` behavior.
- Existing SQLite modules must continue to open.
- Preserve UTF-8. Offsets into `std::string` are UTF-8 byte offsets.
- Strong identifiers and morphology are distinct concepts.
- Morphology is currently audited but not persisted.
- Complex OSIS ranges are partially supported. Never fabricate range endpoints.
- Unknown inline elements preserve safe visible text.
- Unknown structural wrappers may traverse valid descendants.
- Failed imports are atomic and leave no temporary files.
- The writer owns transactions, FTS construction, capability metadata, offset
  validation, rollback, and atomic final rename.
- OSIS is production-ready for its documented subset. Do not reopen validated
  behavior without concrete regression evidence.

## Implementation rules

Before editing, locate the implementation and its tests, understand ownership,
and search for existing helpers. Prefer the smallest coherent change.

Do not:

- add hacks merely to pass tests;
- weaken tests;
- remove validated behavior;
- duplicate neutral logic;
- add source-specific workarounds such as `if (source_format == "osis")` to
  neutral layers.

## Tests

A task is not complete merely because it compiles. For behavior changes:

- add or update a regression test;
- run the directly relevant test;
- run related regressions;
- run broader tests when a shared layer changes.

For neutral content, validate as applicable: plain text, headings, paragraph
state, spans, words, Strong identifiers, footnotes, cross-references, targets,
and capabilities.

For byte ranges validate:

```cpp
start + length <= plainText.size()
plainText.substr(start, length) == word.text
```

Apply equivalent bounds and semantic checks to spans, footnotes, and
cross-references.

## Git safety

Before work, run:

```bash
git status --short
```

Never run automatically:

```bash
git reset --hard
git clean -fd
git checkout -- .
git restore .
git commit
git push
```

Do not rewrite history or revert unrelated user changes. At the end, inspect
the diff and run:

```bash
git diff --check
```

## TASKS.md protocol

Work only on the first main line matching:

```markdown
- [ ] TASK-ID ...
```

Do not begin a second task in the same execution. Change a task to `- [x]`
only when its block also contains `Status: DONE` and objective, real evidence.

Never mark a task complete merely because code was written, compilation
succeeded, behavior was assumed, or only partial tests ran.

## BLOCKED

Use `BLOCKED` only for a real obstacle that cannot be resolved through code,
tests, debugging, local Git, or local documentation. Ordinary compiler and
test failures that can still be investigated are not blockers.

When truly blocked:

- leave the checkbox as `[ ]`;
- set `Status: BLOCKED`;
- record the attempted action, concrete error, evidence, and the human decision
  or information required;
- end the final response with `BLOCKED:` followed by the reason.

## Reporting

Keep the final response brief: task ID, principal change, tests, and result.
