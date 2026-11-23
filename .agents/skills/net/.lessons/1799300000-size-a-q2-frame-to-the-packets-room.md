---
title: Size a Quake II frame to the room the packet has, never to a fixed block
date: 2026-11-22
scope: games/shooters_edge/net/q2_net_server.cpp
concepts: [q2, frame, reliable, channel, packet-size, overflow, area-portal, sounds]
---

The classic channel writes the reliable part first and drops the unreliable part whole when the
two pass the packet, so a 1400-byte frame after a long configstring or print was lost every time.
Ask the channel what the next packet carries (`channel_unreliable_room`: a resend that is due, or
the first waiting message, and whether this frame's reliable data joins it), take away what a Quake
II client cannot read past 1400 bytes after the header, and give the frame that room
(`Q2Wire::room`). `Q2Wire::room` 0 means "no cap", so a computed room of nothing is stored as 1
and the frame floors at 64 bytes. The newer channel cuts long packets into pieces: leave its room 0.

A sound leaves its position out only for a client whose frame holds the entity: the client places
such a sound on the entity, and one it does not hold lands on its own position (our window) or the
entity's stale one (a stock client). Test the frame just written (`w.last`), not the entity's
visibility in general, and keep the position for brush models, whose sound sits at the middle.
