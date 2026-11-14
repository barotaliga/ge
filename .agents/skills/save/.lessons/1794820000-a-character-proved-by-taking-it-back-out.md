---
title: Prove a carried character changed nothing else by taking it back out and comparing bytes
date: 2026-11-14
scope: games/fall_line/sim/profile.cpp, games/fall_line/labs/save_character_lab.cpp, engine/save/part.cpp
concepts: [character, profile, carry, import, extract, since, driver-version]
---

"The rest of the run is the other seed's own" is held two ways, both cheap: carry the profile
into a run, copy it, carry an empty profile back into the copy, and its describe bytes must equal
a fresh run's on the same seed; then step the carried run beside the fresh one and the digests
must never part (they don't when the carried fields are neither stepped nor hashed: the career
lives in the Driver beside the state, and the body is stepped only under the injuries switch).
A field added to a saved type goes in with `io.since(<new version>, field, fallback)` and the
chunk's version constant raised, so older saves still load. Write the character chunk into every
whole-run save too, so `save::extract` can cut a character out of any save; the load ignores it.
