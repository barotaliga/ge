# Agent skills

Each skill is a `SKILL.md` playbook under `.agents/skills/<name>/`, with `name` and
`description` front matter and a `.lessons/` directory that `skill-reflection` fills: one
`<unix-timestamp>-<slug>.md` fragment per lesson, read back with `make lessons` and checked by
`make agents-check`. `make find Q="..."` searches skills, lessons and the code's `ge:` tags at once.
`.claude/skills` and `.gemini/skills` are symlinks to this directory. `AGENTS.md` is the
router that says which skill to load. `tools/check-agents.sh` checks this list, the
router's Skills table and the directories against each other.

The shape is ported from game5's `.agents/skills/`. The procedures are rewritten for a
C++ engine that ports game5 against a read-only TypeScript oracle.

## Session workflow

| Skill | One line |
| --- | --- |
| `start-work` | The preflight: clean tree, sync with main, clone the oracle, name the milestone row |
| `write-code` | File shape, naming, the `ge:` discovery tags, data-first design, comments, the edit loop |
| `commit` | Gates, the public-repo check, commit, PR, green then merge |
| `skill-reflection` | Read lessons first; record, fix, prune, merge, promote and re-key at the end |

## Craft

| Skill | One line |
| --- | --- |
| `determinism` | Bits, not closeness: FP flags, the libm rule, JS semantics, parity failures |
| `port` | The bottom-up procedure from a TS oracle file to a bit-exact C++ port |
| `testing` | The header harness, golden files from the oracle, CTest |
| `build` | CMake, presets, pinned dependencies, the CI matrix, workflow hygiene |
| `sim` | Fall Line's headless simulation: mapgen, the step, the bot, the run clock |
| `platform` | SDL3: window, input, gamepad, audio device, files, focus |
| `gpu` | webgpu.h: device, surface, resources, pipelines, WGSL |
| `render` | The draw passes game5 needs, the pass shape, checking against game5's pictures |
| `audio` | The synth's DSP graph, the beds, the audio thread, parity by meter |
| `ui` | The 2D layer and text, the HUD and cards, the strings table, the dev UI |
| `perf` | Budgets, measuring, the proofs a speed-up owes, the levers |
| `agent-interface` | Diagnostics as prompts, consumer skills, the `ge` CLI |
