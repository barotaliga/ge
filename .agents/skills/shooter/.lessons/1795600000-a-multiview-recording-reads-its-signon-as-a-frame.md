---
title: A multiview recording's first frame read is its signon alone
date: 2026-11-01
scope: games/shooters_edge/client/listen_record.cpp, games/shooters_edge/client/demo_multiview.cpp, games/shooters_edge/labs/se_record_lab.cpp
concepts: [multiview, record, signon, playback, frame-alignment, zstd-flush]
---

The multiview reader closes a frame at the next record whose milliseconds are not 0, so the signon's
records (written at 0 ms) come back as a frame of their own before the first recorded one: a
recording of N server frames reads N + 1 frames, and recorded frame k is read frame k + 1. Compare a
playback with what the server ran off by that one, or every position looks tens of units out. A
packed recorder flushing only by bytes (every MiB) holds a whole short match inside zstd until the
end, so a crash loses all of it: flush by the play's time too (`kListenFlushSeconds`, 2 s, about 1%
of the ratio), and chart the file growing to see it (`se-record`'s sizes shot).
