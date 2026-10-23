---
type: Added
title: The paramotor's acro
---

The paramotor's bar now flies figures, in the new `plugins/paramotor/acro.hpp`:

- **The dive.** The bar forward pulls the front risers and pitches the wing over: steep and fast, with the leading edge held open.
- **The swing-up and the loop.** The bar back pulls the rear risers, biting as hard as the pilot was fast when he pulled. From a cruise they only slow and lift the wing. From a big dive they bend the pair's path up and carry the wing round over him in a loop (`ParaState::loops`, `ParaEvents::looped`).
- **The slack lines and the tangle.** Pulled up without the speed to go over, the lines go slack at the top and he falls into his wing: the new mode `ParaMode::tangled` (`ParaEvents::tangled`). The motor runs down and he falls in the bundle until the release cuts it away or the snow stops him.
- **Flying backwards.** A wing met by the air from behind collapses whole, and one pulled past 1.8 g is held full.

The `para` lab gains five scenarios (dive, swing-up, loop, tangle and cut-tangle) and the columns loops, top g, tangled s and top sink. Fall Line's free ride says `loop` and `tangle` as para events; the player flies the figures on his lean. Every paramotor digest moves, because the state grew. Every other number the free ride reports is unchanged.
