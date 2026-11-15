---
type: Added
title: ge mod check --run plays a mod in its game and replays it
---

`ge mod check <package> --run` hands the package to the game's own CLI (`secli mod run <package>` for Shooter's Edge), which plays the game's mod scenario once without it and twice with it. The package is refused when the two runs part, naming the first frame; otherwise the answer says what the mod changed, how many hooks it ran and their share of the match's time. `--runner=<file or folder>` names the game's CLI; by default it is the one built beside `ge`, then the PATH.
