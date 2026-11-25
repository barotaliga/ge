---
type: Added
title: sv_modern on QuakeWorld and Quake II servers
---

`sv_modern 1` on a Shooter's Edge server turns on what a modern server judges by, in each game's own terms: QuakeWorld rewinds every shot's line and every rocket, grenade and nail to where the shooter saw the others (`sv_modern 2` also lifts the rate cap on a LAN); Quake II does the same and rewinds one frame further, as the client drew them. A setting written anywhere else still wins, and `sv_modern 0` puts back what it set. A Quake II client also takes its map from the server's own HTTPS files, trusted by the certificate the server names.
