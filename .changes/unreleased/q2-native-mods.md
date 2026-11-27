---
type: Added
title: Quake II's servers host its existing mods
---

A server of Quake II started with a mod folder (`sesv +set fs_basegame q2 --mod=<folder>`) now loads that mod's own game library, built for this platform, and runs it unchanged: an unmodified client joins and plays the mod's rules, HUD and weapons on our server. A mod folder without a library plays our own rules on its maps and files.
