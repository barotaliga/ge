---
type: Added
title: The dedicated server runs a mod from its own folder
---

`sesv --mod=<folder>` reads the maps and files of that folder over the base game's and runs the folder's own multiplayer program in place of the baseline mod; serverdata and the server info's `*gamedir` name the folder, so a classic client mounts it too. A game folder now also mounts its unnumbered packs (a mod's `misc.pak`) after the numbered ones, and the progs checker takes a builtin declared with -1 parameters.
