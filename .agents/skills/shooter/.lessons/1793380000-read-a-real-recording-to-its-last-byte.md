---
title: Read a real recording to its last byte before trusting a format's description
date: 2026-10-30
scope: games/shooters_edge/client/demo_multiview.cpp, games/shooters_edge/client/demo_client.cpp
concepts: [multiview, recording, setangle, detect, record-layout, gzip]
---

The multiview reader matched its description and still stopped 57 KB into a real recording: a
`setangle` sent to everyone carries the player it turns as a byte before the angles, which no
description of the message said. Reading a real file to its end (records, frames and clock
counted against the file's size) is what found it; a reader written only against its own writer
agrees with itself. Detect a format from its first record or two, never a long chain: a damaged
record ten records in made the detector call the file another format and the error named the
wrong reader. Keep the real recording out of the tree and run it with `make lab LAB=se-demo
ARGS=--file=<path>`.
