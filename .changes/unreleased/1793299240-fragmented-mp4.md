---
type: Added
title: Fragmented MP4: captures and exports written as .mp4 by the engine's own muxer
---

`engine/capture/mp4.hpp` writes the same AV1 and Opus tracks as the WebM export in a fragmented MP4 (`ftyp`, a `moov` with no samples, then `moof` and `mdat` fragments cut at keyframes, an `mfra` at the close), so a file cut short by a crash still plays to its last whole fragment. Opus's pre-skip is the sound track's edit list; the sample entries for H.264 (`avc1`) and HEVC (`hvc1`) are ready for the platforms' encoders. `capture/mux.hpp` picks the container by the file's name, so `fall_line --capture=run.mp4` and an export to an `.mp4` both use it. `capture/mp4_read.hpp` reads the files back for the tests and the `capture` lab, where ffmpeg decodes every picture of each MP4 and plays a file cut halfway through its last fragment at least as far as the engine's reader keeps.
