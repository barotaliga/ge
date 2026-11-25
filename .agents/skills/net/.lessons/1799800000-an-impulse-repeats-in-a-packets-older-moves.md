---
title: An impulse repeats in a packet's older moves, so act only on the newest
date: 2026-11-24
scope: games/shooters_edge/net/qw_server.cpp
concepts: [impulse, move-packet, captain-mode, qw-captains, server-hooks, election]
---

A QuakeWorld move packet carries three moves, the newest last, and our client repeats an impulse
in the next packets' older moves (`c2s_impulse_backup`) so a lost packet does not lose it. A
server hook that ACTS on an impulse (captain mode's pick) must act only on the newest move's
(`fresh`) and only swallow the repeats, or one key press picks twice. `setinfo` never reaches
`game_command`: a game that refuses a user info change (a team locked while picking) needs the
server's `info_rule` hook, and must put the key back in the client's own settings by stuffing
`setinfo`, or the client sends the refused value again on its next change. Roles a player asks
the others for (captain, coach, admin) share ONE `Election` the server owns, its role as data, so
"one election at a time" holds without the books checking each other.
