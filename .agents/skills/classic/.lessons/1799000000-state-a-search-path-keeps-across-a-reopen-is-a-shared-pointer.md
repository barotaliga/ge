---
title: State the search path keeps across a reopen (links, map groups) is a shared pointer open_game_dir carries over
date: 2026-11-22
scope: plugins/classic/content/files.cpp
concepts: [search-path, fs-restart, links, open-game-dir, find-file, file-words]
---

`open_game_dir` starts from a fresh `SearchPath`, so anything a player or a console set on the path
(the file links, the map groups) would vanish at `fs_restart` or a game change. Keep such state in a
`std::shared_ptr` member and move it across in `open_game_dir`; copies of the path (the server's
download reader, a lab's) then share it, and a word may change it through a const path.

A rewrite that applies to every read belongs in `find_file` itself, behind a check that the lists
are empty, so the common lookup costs nothing more and every caller (maps, sounds, downloads) sees
the link without being told.

Testing the Quake II server's filter words over rcon: with `sv_banmode` 0 only the list joins, and
that refuses the administrator's own address too, so its rcon goes unanswered. Run the last words
through `q2_admin` directly.
