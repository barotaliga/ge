---
type: Added
title: See who is on a server, and grant the easy jump to one player
---

`users` lists every player on a server by user id, and any player may ask it. `status` is the administrator's table: each player's id, ping, loss, address and easy jump. `speedjump <id|name> on|off|<share>` grants the easy jump to one chosen player, for example a player whose disability makes strafing hard, while `pm_speedjump` stays as it is for everyone else. The grant is disclosed: it is the server-only `*speedjump` key in that player's user info, every client sees it, and everyone is told. In the window these are console commands for the match you host. On a dedicated server, set `rcon_password` and send `rcon <password> <line>` from a joined window or any classic client.
