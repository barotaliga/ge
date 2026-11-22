---
title: A long answer on the QuakeWorld reliable channel is queued and paced, never sent at once
date: 2026-11-22
scope: games/shooters_edge/net/qw_sheet.cpp, games/shooters_edge/net/qw_game.cpp
concepts: [reliable-channel, kbackbuffers, ge-sheet, settings-sheet, paced-send, extension-bit]
---

The settings sheet is ~80 rows; written in one frame it filled the channel's back buffers and
the client saw eight rows. `QwSheetOut` holds the parts and `qw_sheet_pump` sends a few each frame
while `pending_count` is under 3; a new sheet replaces what of the last has not gone. A new svc
also moves `tests/golden/shooters_edge_qw_messages.txt` (rows added only) and is stripped from
recordings like every `ge` message a stock player would not read. Stock Quake II servers broadcast
an unknown command as chat, so a reader there only sends words the server is known to take.
