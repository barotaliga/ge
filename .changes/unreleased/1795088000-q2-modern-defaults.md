---
type: Changed
title: Quake II's server takes the modern defaults
---

A server of Quake II now holds each client's rate between 15000 and 60000 bytes a second (it was capped at 15000), drops a silent client after 90 seconds (was 125) and checks move time (`sv_speedcheck` 1) by default. The new `sv_lanrate 1`, on either game's server, lets a client on the local network play at whatever rate it asks for. The modern server's names `sv_min_rate`, `sv_max_rate` and `sv_lan_force_rate` work as the same settings.
