---
title: Pace a Quake II server's reliable bulk by the backlog's bytes, never by pending messages
date: 2026-11-22
scope: games/shooters_edge/net/q2_votes.cpp
concepts: [q2, reliable, channel, packet-size, pacing, settings-sheet, svc-120, ge-ext]
---

The classic channel packs reliable data into pending buffers of up to 1434 bytes, QuakeWorld's
size, but a Quake II client reads a packet of 1400 at most (the window drops with "the block
length 1426 ... is not a block's"). Counting `pending_count` let two 700-byte rows share one
buffer. Send bulk reliable data only while `channel_reliable_backlog` plus the part stays under
about 640 bytes: the reliable part and the frame after it then fit. One message goes a server
frame, so 60 sheet rows take about two seconds; a test waits on the result, not a step count.

Quake II's server never answers a client's `*ge_ext` bits, so a feature a stock server must not
see is OFFERED by the server first (an empty sheet as the player enters) and the client speaks
only after the offer: a stock Quake II game echoes an unknown command to everyone as chat. A
window reading an offered-then-asked sheet takes it only once `asked` is cleared by the end part,
or it shows the empty offer.
