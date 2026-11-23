---
type: Added
title: Two-ear sound for headphones, and the folder the game writes in
---

The engine's synth can hear each sound by two ears (`Synth::binaural`): the ear away from a sound
hears it a little later, softer and duller. Quake II's `al_hrtf` 2 (our `s_hrtf`) turns it on.
`+set homedir <folder>` on the command line picks the folder every game writes in.
