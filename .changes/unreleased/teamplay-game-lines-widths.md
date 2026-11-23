---
type: Added
title: Message triggers on your own game, and fixed-width team macros
---

Message triggers (`msg_trigger`, with their levels, and the regex triggers) now fire on the lines of a game you host in the window, not only on a server you join or a demo. A team message's `$` words can be given a fixed width: `set tp_length_health 3` pads or cuts `$health` to three letters, and `set tp_align_health r` (or `c`) sets it right or centred. Colour codes and braces take no room.
