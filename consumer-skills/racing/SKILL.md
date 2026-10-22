---
name: racing
description: "Use when the game links ge::racing (`ge use racing`): the control tape a ghost and a replay are kept on, the record book's policy (what beats a record, split gaps), and the field's standings."
plugin: racing
---

# The racing plugin

`ge::racing` (headers `racing/*.hpp`, namespace `ge::racing`) is what any racing game needs
whatever it races. It names the mechanism (a vehicle, a checkpoint, a tape schema the game
fills with its own axes), never a game's words, so the game supplies the meaning.

| Header | What | Start with |
| --- | --- | --- |
| `racing/tape.hpp` | The control tape: each axis on a fixed grid, run-length encoded, base64 streams; the recorder, the reader, the map fingerprint | `ge explain racing/tape.hpp` |
| `racing/records.hpp` | The record book's policy: `beats` (lower wins, never a tie), `split_gap`, `note_record` | `ge explain beats` |
| `racing/standings.hpp` | The field's order: home first by the clock, then by progress (`leg_progress`, `field_order`, `place_among`) | `ge explain field_order` |

## Use it

- **A ghost or a replay is the inputs, not the positions.** Snap every input to the tape's
  grid where it is made, so the run the simulation steps IS the run the tape replays; then a
  replay of the tape through the same `step` gives the same digest (skill: `sim-system`).
- **The tape's bytes are a format.** A stored ghost from one engine release reads in the next
  unless the release notes say the tape's format broke. Keep a golden tape in `tests/`.
- **The game decides what beats what** only through `beats`: a record, a split and a finish
  plate all read it, so they never disagree.
- Find the rest with `ge find racing <word>`; every header's leading comment is its manual.
