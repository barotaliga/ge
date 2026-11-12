---
type: Added
title: an input log and its replay file are the same run
---

`spectate_pack` writes a run's input log as a replay file (the stream its broadcast would have been), and `spectate_inputs` reads a stream that starts at its first block back into the same log (`engine/replay/spectate.hpp`).
