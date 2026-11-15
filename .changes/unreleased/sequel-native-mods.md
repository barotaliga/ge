---
type: Added
title: The sequel's servers host its existing mods
---

A server of the sequel started with a mod folder (`sesv -game 2 --mod=<folder>`) now loads that mod's own game library, built for this platform, and runs it unchanged: an unmodified client joins and plays the mod's rules, HUD and weapons on our server. A mod folder without a library plays our own rules on its maps and files.
