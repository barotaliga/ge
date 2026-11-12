---
type: Changed
title: spectating by inputs sends keyframes as deltas
---

A run broadcast by its inputs (`engine/replay/spectate.hpp`) now sends every 30th keyframe as a whole save and the rest as deltas: the save XORed with the one before it, which the block's zstd packs to about 1.5 kB where a whole save takes about 80 kB. `SpectateOptions::full_every` sets how often a save is whole. A viewer who joins rebuilds the newest save from the whole one and the deltas after it and follows from there (`Spectator::skip_ahead`). Fall Line's broadcast over two minutes falls from 9.2 kB to 1.2 kB a second.
