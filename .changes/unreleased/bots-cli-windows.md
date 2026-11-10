---
type: Fixed
title: the headless bot team builds on Windows, and the installed headers compile together
---

`shooters_edge_bots` reads its console lines from stdin on a thread of its own instead of polling the descriptor, so it needs no POSIX header and builds with MSVC. The shooter plugin's aim target is `ge::shooter::MovingTarget`: it shared the name `Mover` with the move's own state in `slide.hpp`, which broke any file that includes every installed header.
