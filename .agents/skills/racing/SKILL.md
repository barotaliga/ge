---
name: racing
description: "Use for plugins/racing in gameengine2: the racing primitives any racing game links. The control tape a ghost and a replay are kept on, the record book's policy, and the field's standings."
---

# The racing plugin

`plugins/racing/` (`ge_racing`, namespace `ge::racing`) holds what every racing game needs
whatever it races: the controls a run was ridden on, what beats a record, and who leads. It
depends on the engine's core only (`ge_add_plugin(racing ENGINE core)`). It names the
mechanism for any racer (a `vehicle`, a `checkpoint`, a `TapeSchema` the game fills with its own
axes), never one game's words (skier, gate, sled).

| File | What |
| --- | --- |
| `racing/tape.hpp/.cpp` | The control tape: axes on a fixed grid, RLE + base64 streams, the recorder, the reader, the map fingerprint |
| `racing/records.hpp` | The record book's policy: `beats`, `split_gap`, `note_record` |
| `racing/standings.hpp` | The field's order: `leg_progress`, `field_order`, `place_among` |

## Rules

- **Bytes, not closeness.** The tape's byte format is stated once in `tape.hpp`'s header. A
  change to it orphans every stored ghost and replay, so it is a breaking change.
- Tests: `tests/racing_*_test.cpp` against `tests/golden/racing_*.txt` (`make golden`).
- Still to come: the stored-JSON readers for the record book and the tape. They land with the
  ghost and record stores (M10), here beside the policy they read for.
