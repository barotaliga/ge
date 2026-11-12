---
type: Added
title: the server as a TV source
---

A game on the server core can now be broadcast as it runs (`engine/tv/server_source.hpp`). A game that fills `ServerGame::tv_frame` and `tv_keyframe` has its frames and periodic keyframes written as a replay stream, which a TV node takes as its source. The core's sample game broadcasts in about 300 bytes a second, and `SampleWatch` follows it to the server's digest. The `tv` lab can use it as its match (`--source=sample`).
