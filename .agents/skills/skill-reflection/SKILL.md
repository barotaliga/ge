---
name: skill-reflection
description: "Use at BOTH ends of any gameengine2 session that loads another skill. At the START, read that skill's lessons, filtered to the paths and concepts the task touches. At the END, before the commit: record new lessons, fix what the skill said that was wrong, delete stale lessons, merge duplicates, promote what is true every time into SKILL.md, feed search misses back into the ge:keywords, and keep AGENTS.md a router. Owns the lesson format, the size bars and tools/skill-lessons.mjs."
---

# Skill reflection

Every other skill is a playbook that improves only if a session improves it. This skill is
how. It runs **twice per session**, once before the work and once before the commit, and it
is the only skill allowed to rewrite another skill's `SKILL.md`.

**A session that loaded a skill owes that skill a reflection pass.** The trap you hit
halfway through is the trap the next session hits too, unless you write it down.

An empty `.lessons/` (a `.gitkeep` alone) is the state every skill starts in, not a broken
tool. Read such a skill from its `SKILL.md` alone, and reflect into it anyway.

| When | Do |
| --- | --- |
| **OPEN**, right after loading a skill | Read its lessons, narrowed to what the task touches. Seconds. |
| **CLOSE**, before the commit | Record, correct, prune, merge, promote, re-key. Same PR as the work that taught it. |

Both halves run for **every** skill the session loaded, not only the main one.

## OPEN: read before you work

```sh
make lessons                                          # inventory: every skill, its count, its verdict
make lessons ARGS="port --list"                       # one skill's index: file, words, scope, concepts, title
make lessons ARGS="port"                              # its lessons in full, oldest first
make lessons ARGS="--scope=engine/core/noise.hpp"     # across every skill, by the path you will edit
make lessons ARGS="determinism --concepts=libm,fma"   # by concept
make find Q="heightfield sampleField"                 # files, skills AND lessons matching any word
```

- **A lesson with no `scope` is global**: it answers every `--scope` query.
- **`--scope` matches a path prefix in either direction.** A lesson scoped `engine/core`
  answers `--scope=engine/core/noise.hpp` and `--scope=engine` alike.
- **`--concepts` is OR within the flag, AND with `--scope`.** `--vocab` lists the tags in use.

Read the lessons before the first edit, not after the first surprise.

## The fragment format

```
.agents/skills/<skill>/.lessons/<unix-timestamp>-<slug>.md      # timestamp: $(date +%s)
```

```markdown
---
title: The claim the next session must know, in one line
date: YYYY-MM-DD
scope: engine/core/noise.hpp, tools/parity/golden-core.mjs
concepts: [noise, golden, operand-order]
---

The lesson: concrete and self-contained, written for a session that has not seen the pass
that taught it. Name the files and commands involved and the failure it prevents.
```

| Field | Required | Meaning |
| --- | --- | --- |
| `title` | yes | A CLAIM, not a topic: "A reordered sum is a different double, so keep the oracle's operand order", never "About sums". |
| `date` | yes | `YYYY-MM-DD`, the day it was learned. A merge keeps the oldest source's date. |
| `scope` | no | Repo-relative paths, comma-separated, where it stays true. Omitted means global. |
| `concepts` | no | Lower-case kebab tags: what a future task would search for. Reuse a tag from `--vocab` over a synonym. |

- One lesson per file. Separate files are why parallel sessions never conflict, so a
  session records a lesson as a fragment, never by editing a `SKILL.md` mid-task.
- `make agents-check` (CI's `agents` job) runs `tools/skill-lessons.mjs --check`: a bad
  filename, a missing title or date, a non-kebab concept or an empty body fails it. A scope
  whose path no longer exists is a warning: it is the best staleness signal there is.
- Scope to the **narrowest** path where the lesson stays true. A lesson scoped `engine/` should
  have had no scope.
- Lessons are public like everything else in this repo.

## CLOSE: reflect before you commit

Six questions, in this order, for every skill the session loaded.

### 1. What did this session learn? Record it.

A trap, a measurement, a wrong turn and its fix, a step you wish you had known. Write it in
the same PR as the work that taught it. Name what went wrong AND what went right: a pass that
found the answer on the third try owes a lesson naming the first two. A pass where the
skill's steps worked exactly as written owes nothing; silence is a legitimate outcome.

### 2. Did the skill say something WRONG? Fix the `SKILL.md` in place.

A command that does not exist, a path that moved, a claim the session disproved. That is a
bug in the playbook, not a lesson: fix the line where it stands, in this PR. These skills were
written before most of the code existed, so expect this often.

### 3. Has a lesson gone stale? Delete it.

The trap got a check, the manual step became a command, the file it warned about is gone.
Delete it; git remembers. `--check` names every lesson whose scope vanished, but a lesson can
rot without its paths moving, so reread the ones your task touched.

**Prefer removing the trap to recording it.** When a lesson describes a tool that misleads
(a gate that skips new files, a check that passes locally and fails on CI), fixing the tool
is better than a lesson, and the fix deletes the lesson.

### 4. Do two lessons say the same thing? Merge them.

One fragment: a new filename timestamp, the oldest `date`, the union of `scope` and
`concepts`, a body that covers every case. Delete the sources. Merge synonym concepts too.

### 5. Is a lesson true EVERY time? Promote it into `SKILL.md`.

The most valuable step. A lesson every run of the skill must obey is an instruction; left as
a lesson, every session pays to rediscover that it matters. The test is literal: would it
apply in 100% of this skill's runs? Then write it into the skill at the right place (a step,
a checklist row) and delete the fragment. A lesson that applies to most runs stays a fragment
with a `scope` saying where.

### 6. Did a search miss? Re-key the code.

Agents find code by grep and by `make find`. If you searched for a word and did not land on
the file that owned the answer, that is a bug in the file's `ge:keywords` (the `write-code`
skill, § Discovery tags): add the word you searched for first, run `make index`, and commit it.
Likewise a skill whose `description` lacks the words you used to look for it gets them. This
is how the index learns the vocabulary agents actually use.

## The size bars

Everything a skill carries is read by every session that loads it, and a wall of text gets
skimmed. `tools/skill-lessons.mjs` measures in words:

| Bar | Limit | Over it means |
| --- | --- | --- |
| One fragment's body | 350 | Two lessons, or one wrapped in the session's story |
| A skill's lessons, all together | 4000 | Merge and promote |
| Lessons on one skill | 15 | The same call, counted the other way |
| A `SKILL.md` | 5000 | It repeats itself or has absorbed a reference doc |

A bar is a prompt to consolidate, never a licence to truncate. Settle the fragments this
session wrote or touched. When the inventory says CONSOLIDATE, run questions 3 to 5 across the
skill's whole set (`--list`, `--vocab`, `--check`) as **its own commit**, separate from the work.

## Does it belong in AGENTS.md at all?

`AGENTS.md` is read at the start of every session, so it is the most expensive text in the
repo. It is a ROUTER. A rule earns a place there only if it is true for essentially every
task, or it bites before a session knows which skill to load.

| The content is | It goes |
| --- | --- |
| A rule only one kind of task needs | that task's skill; AGENTS.md keeps a pointer row |
| A procedure with steps or commands | a skill |
| A trap that bites before any skill is loaded | stays in AGENTS.md |
| A pointer table (which doc, which skill, where code goes) | stays in AGENTS.md |
| A long reference catalog | a `docs/` page (`docs/index.md` is generated from the code) |

Move text into an existing skill wherever one fits. A new skill adds a router row and
`tools/check-agents.sh` holds it to `AGENTS.md` and `.agents/skills/README.md`. A pointer left
behind names the skill by name, and moved text is never kept in both places.

## Checklist

- [ ] OPEN: each loaded skill's lessons read, narrowed by `--scope` or `--concepts`
- [ ] CLOSE: a fragment for each thing learned, with `scope` and `concepts`
- [ ] CLOSE: what the skill said wrong, fixed in its `SKILL.md`
- [ ] CLOSE: stale lessons deleted, duplicates merged, the always-true promoted
- [ ] CLOSE: each search miss added to the file's `ge:keywords`, `make index` run
- [ ] CLOSE: nothing appended to `AGENTS.md` that a skill should own
- [ ] `make agents-check` green
