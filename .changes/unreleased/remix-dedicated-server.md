---
type: Added
title: Classic remix on a dedicated server
---

A dedicated server now plays classic remix. Set the same `cr_` settings the game's own match uses (`cr_enabled`, `cr_items`, `cr_models`, `cr_physics`, `cr_sounds`, `cr_monsters` and the rest) in its config or on its command line, and point the other full game's `fs_basedir_q1` or `fs_basedir_q2` at its install. The server then plays that game's maps and wears its weapons, and players who join see the same remix.
