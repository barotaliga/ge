---
type: Fixed
title: Joining a stock Quake II server shows the game
---

Joining a stock Quake II server from the window while its own map still loaded behind the title left the full console down over the joined game until Escape raised it; the console now rises as soon as the game is joined (or a demo plays). The joined world is drawn with Quake II's own palette and textures instead of grey and noise, in demo videos too. Each line and effect the server sends is heard once (a player's "entered the game" no longer repeats with every packet, and spawn effects no longer pile up). `+connect` on the command line joins a Quake II server as Quake II, and a server's version check (`$version`) is answered with the window's version.
