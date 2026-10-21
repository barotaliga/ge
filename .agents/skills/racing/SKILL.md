---
name: racing
description: "Use for plugins/racing in gameengine2: the racing primitives any racing game links (the control tape a ghost and a replay are kept on, the record book's policy, the field's standings), ported from oss-game-framework src/racing bit and byte for byte."
---

# The racing plugin

`plugins/racing/` (`ge_racing`, namespace `ge::racing`) is oss-game-framework's `src/racing/`
in C++. It depends on the engine's core only (`ge_add_plugin(racing ENGINE core)`). The
framework names it for any racer, and so does this plugin: a `vehicle`, a `checkpoint`, a
`TapeSchema` the game fills with its own axes. No game's words (skier, gate, sled) go in it.

| File | Oracle | What |
| --- | --- | --- |
| `racing/tape.hpp/.cpp` | `src/racing/tape.ts` | The control tape: axes on a fixed grid, RLE + base64 streams, the recorder, the reader, the map fingerprint |
| `racing/records.hpp` | `src/racing/records.ts` | The record book's policy: `beats`, `split_gap`, `note_record` |
| `racing/standings.hpp` | `src/racing/standings.ts` | The field's order: `leg_progress`, `field_order`, `place_among` |

## Rules

- **Bytes, not closeness.** A tape game5 stored decodes here and re-encodes to the same text.
  The byte format is stated once in `tape.hpp`'s header; changing it orphans every stored ghost.
- Parity: `tests/racing_*_test.cpp` against `tests/golden/racing_*.txt`, written by
  `tools/parity/golden-racing.mjs` (`make golden`).
- Not ported yet: `readBook` and `isControlTape`, the stored-JSON readers. They come with the
  ghost and record stores (M10) and belong here, beside the policy they read for.
- Porting more of the framework's racing code: the `port` skill's procedure, with this plugin as
  the destination.
