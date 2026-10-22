---
name: start-work
description: "Use before any task in a game repo built on the engine: a clean tree synced with main, the repo green against the engine's rules (make agents-check), the skill that owns the subject loaded and its lessons read."
---

# Start work

A session that starts on a dirty tree, a stale branch or a red check spends its first hour on
someone else's problem. Five minutes here prevent that.

## Steps

1. **A clean tree on a fresh branch.**

   ```sh
   git status                      # nothing staged, nothing modified
   git fetch origin main
   git switch -c <topic> origin/main   # or rebase your branch onto origin/main
   ```

2. **The repo is green before you touch it.**

   ```sh
   make agents-check   # ge doctor: guidance, skills, engine pin, flags, sources, messages
   make test           # every test, every lab at its smoke flags
   ```

   A red check you did not cause is still the first thing to fix or report: work on top of it
   hides your own breakage.

3. **Load the skill that owns the subject** (`AGENTS.md` § Skills routes it), then
   `write-code` for any code change, then read their lessons before the first edit:

   ```sh
   make lessons ARGS=<skill>                      # all of them, oldest first
   make lessons ARGS="<skill> --scope=sim/"       # only those whose scope overlaps a path
   ```

4. **Survey before you build.** Grep the `ge:` tags for the concept
   (`grep -rn "ge:keywords.*<word>" sim labs tests`) and read what exists. The engine's own
   headers are in `build/<preset>/_deps/gameengine2-src/`; each carries the same tags. Build on
   what the engine already gives (skill: `engine`) before writing your own.

5. **Name the done criterion**: the test or the lab output that will show the task is done.

## Checklist

- [ ] Clean tree, branch off a fresh `origin/main`
- [ ] `make agents-check` and `make test` green before the first edit
- [ ] Subject skill and `write-code` loaded, lessons read
- [ ] Existing code and the engine's API surveyed
- [ ] Done criterion named
