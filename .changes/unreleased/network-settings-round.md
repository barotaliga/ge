---
type: Added
title: Packets and losses shown on the console, rcon to a server you have not joined, and solid players in prediction
---

`showpackets 1` shows each packet to and from the server you joined on the console (2: those sent, 3: those received), and `showdrop 1` says when packets were lost or came out of order.

With `rcon_address` set, `rcon` reaches that server while you are not joined to one, and its answers appear on the console. Joined to a Quake II server, `rcon` now reaches it.

`cl_solid_players 0` lets your predicted moves pass through the other players on a QuakeWorld server.
