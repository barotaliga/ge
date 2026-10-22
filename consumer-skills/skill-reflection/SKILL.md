---
name: skill-reflection
description: "Use at both ends of any session that loads another skill in a game repo: read that skill's lessons first (ge lessons), and before the commit record what the session learned as lesson fragments, fix what the repo's own skills got wrong, prune and merge lessons, promote what is always true, and feed search misses back into the ge: keywords."
---

# Skill reflection

Skills improve only if sessions improve them. This one runs **twice per session**, before the
work and before the commit, for every skill the session loaded.

## OPEN: read before you work

```sh
ge lessons                                  # every skill and its lesson count
ge lessons <skill>                          # its lessons in full, oldest first
ge lessons <skill> --scope=sim/             # only those whose scope overlaps the path
```

A lesson with no `scope` is global and always shows.

## The fragment format

One lesson per file: `.agents/skills/<skill>/.lessons/<unix-timestamp>-<slug>.md`
(timestamp: `date +%s`).

```markdown
---
title: The claim the next session must know, in one line
date: YYYY-MM-DD
scope: sim/game.cpp, labs/
concepts: [determinism, replay]
---

The lesson: concrete and self-contained, naming the files and commands involved and the failure
it prevents.
```

`title` and `date` are required; `scope` (repo-relative paths) and `concepts` (lower-case kebab)
are optional. `ge doctor` fails a bad file name, a missing title or date, a bad concept
or an empty body.

## Two kinds of skill

- **The repo's own skills** are yours: fix their `SKILL.md` in place.
- **The engine's skills** (the `ge:skills` block in `AGENTS.md`) are rewritten by
  `ge skills sync` on every engine upgrade. Never edit their `SKILL.md`: `ge doctor` fails
  an edited one. Their `.lessons/` are this repo's and survive every sync, so what you learn
  about an engine skill goes there. A lesson that would hold in EVERY game on the engine belongs
  upstream: open an issue on the engine's repository quoting it, so a release can promote it into
  the skill itself.

## CLOSE: before the commit

For every skill the session loaded:

1. **What did this session learn?** A trap, a wrong turn and its fix, a step you wish you had
   known: one fragment each, in the same PR as the work.
2. **Did the skill say something wrong?** In the repo's own skill, fix the line. In an engine
   skill, write a lesson that corrects it and report it upstream.
3. **Has a lesson gone stale?** The trap got a check, the file is gone: delete it.
4. **Do two lessons say the same thing?** Merge them into one fragment: the oldest date, the
   union of scope and concepts.
5. **Is a lesson true every time?** In the repo's own skill, promote it into `SKILL.md` and
   delete the fragment.
6. **Did a search miss?** Add the word you searched for to the right file's `ge:keywords`.

Keep a fragment under about 350 words and a skill's lessons under about 15; past that, merge and
promote. Lessons are part of the repo, as public as the code.

## AGENTS.md is a router

It is read at the start of every session. A rule earns a line there only if it holds for nearly
every task; a procedure goes in a skill, and `AGENTS.md` routes to it. A new skill of the repo's
own gets a row in its own-skills table, outside the `ge:skills` block.
