---
type: Added
title: Players' advanced configs run as written
---

The console reads more of the configs players carry: `if` without `then` after a comparison, `=`, `<>`, regular expressions (`=~`, `!~`), the expression form `if (...) then` with `eval` and `set_eval`, `if_exists`, `set_calc`'s `div`, `and`, `or`, `xor`, `pos`, `tobrown` and `towhite` (a division by zero now stores 999, as old configs expect), and older key names such as `uparrow`, `lctrl`, `kp_home` and `~`. A corpus of five players' configs is held golden with the config each saves (`tests/golden/console_configs/`).
