---
title: A demo frame's sounds and prints last one frame; a draw that reads several must gather them
date: 2026-11-01
scope: games/shooters_edge/client/timedemo.cpp, games/shooters_edge/client/demo_heard.cpp, games/shooters_edge/client/main.cpp
concepts: [demo, playdemo, sounds, prints, chat, demo-heard, seek, print-lines]
---

`DemoState::started`, `stopped`, `voice`, `prints` and `centre` are emptied by the next
`next_frame`, and `playdemo_step` reads every demo frame up to the clock (several a draw at speed
or on a slow frame), so reading them off the state after the step drops most of them. Gather them
in `timedemo_step` while `Timedemo::hear` is on (`DemoHeard`), drain once per draw, and clear in
`demo_seek` so a seek plays nothing it skipped. Keep `hear` off for the benchmark and the labs, so
nothing grows unread. Real chat carries more than the charset's high-bit letters: players' clients
write `{&cRGB}` colour marks and braces, which `print_lines` takes out; check on a real recording
(`se-demo --file=...` prints the first death and chat lines).
