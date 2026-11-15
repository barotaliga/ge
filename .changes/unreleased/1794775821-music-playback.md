---
type: Added
title: Music played from the kept soundtrack
---

`engine/audio/music.hpp` plays a game's music from its kept FLAC files: decoded off the game thread, its two channels on their own sides on a music fader, with pause and resume in place (`SamplePlay::from` starts a sampled voice partway into its recording). `engine/audio/music_console.hpp` gives any game `bgmvolume` and the `cd` command (`play`, `loop`, `stop`, `pause`, `resume`, `on`, `off`, `info`). Shooter's Edge plays each level's own track, looping, from the game's data folder.
