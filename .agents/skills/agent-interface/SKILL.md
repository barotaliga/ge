---
name: agent-interface
description: "Use for how the engine talks to agents in gameengine2 and in the game repos that consume it: the diagnostics convention, consumer skills, and the ge CLI (ge new, ge skills sync, ge doctor) planned in docs/goals.md M12."
---

# The agent interface

Agents build this engine, and agents build the games that use it (G6). The engine's
output and tooling are written for them.

## Diagnostics (applies now, to every message)

A message is a prompt with three parts:

1. **What** failed, in one line, with the values involved (hex for doubles when
   bits matter).
2. **Where**: file:line, and the game context (seed, step, system, pass).
3. **Next**: the command to run, the skill to load, or the doc to read.

Example: see `docs/architecture.md` § Diagnostics. Do not write bare error codes,
"something went wrong", or a stack trace without a sentence. Asserts use
`GE_ASSERT(cond, "what; next: …")` once `core/output` exists (M2).

## Consumer skills (M12)

Skills a downstream game repo needs: start a game on the engine, add a sim system, add
a draw pass, add a sound, add a card, pin a seed, upgrade the engine, cut a release.
They live in `consumer-skills/<name>/SKILL.md` in this repo, are versioned with the
engine, and are written in the same shape as `.agents/skills/` (front matter and a
`.lessons/` directory).

## The `ge` CLI (M12)

- `ge new <dir>`: scaffold a game repo with the layout, CMake against the released
  engine, `AGENTS.md` and its symlinks, `.agents/skills/` copied from
  `consumer-skills/`, CI and `check-agents.sh`.
- `ge skills sync`: refresh a repo's consumer skills to the engine version it pins,
  without touching that repo's own skills or lessons.
- `ge doctor`: check a repo's engine pin, flags, symlinks and skills, and print each
  finding as a prompt.

Until M12, keep any message, doc or skill you write consistent with this shape, so
the CLI only has to package it.

## Labs

A lab's `fail` lines follow the diagnostics shape, and its report, `report.json` and
contact sheet are written for an agent to read first. `labs play --stdio` (M5c, the protocol
in `games/fall_line/sim/play.hpp` and `docs/labs.md` § Drive labs) is the engine's playtest
interface for agents. M12's `ge new` scaffolds a game's `labs/` directory and ships the `labs` skill to consumers.
