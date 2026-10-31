---
title: A recording reader is proved on real files read to the last byte, formats are told by their bytes, and our server's files go back through the reader
date: 2026-10-30
scope: games/shooters_edge/client/demo.cpp, games/shooters_edge/client/demo_multiview.cpp, games/shooters_edge/client/demo_client.cpp, games/shooters_edge/net/classic_record.cpp, games/shooters_edge/net/classic_svc.cpp
concepts: [demo, recording, format-detection, multiview, record-layout, usercmd-size, clean-slate, round-trip, reader, codec]
---

- A reader written against its own writer agrees with itself. The multiview reader matched its
  description and stopped 57 KB into a real file: a broadcast `setangle` carries a player byte no
  description named. Read a real recording to its end (records, frames and clock counted against
  the file's size) with `make lab LAB=se-demo ARGS=--file=<path>`; keep the file out of the tree.
- Nothing branches on a file extension the clean-slate check refuses: tell formats by their first
  bytes (the single-player demo's track digits and newline; a client recording's finite time and
  record type 0..2), and find a name with no extension by its stem. Settle a padded 24- or 28-byte
  command struct by walking 16 records (a zero float reads as a valid time). Detect from the first
  record or two, never a long chain.
- The codec's round trips prove only that its halves agree. Our server's first recording failed
  the demo reader (`spawnbaseline2`'s entity number, chunked downloads). A recording test reads its
  file back through `client/demo.hpp` to the end; run one whenever the server's messages change.
