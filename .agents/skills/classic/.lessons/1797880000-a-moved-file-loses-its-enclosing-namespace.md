---
title: A file moved from ge::shooter to ge::classic loses the names it used unqualified
date: 2026-11-15
scope: plugins/classic
concepts: [classic, plugins, namespace, move]
---

Code inside `namespace ge::shooter::content` found `Locs`, `parse_locs` and the rest of the
shooter kit by the enclosing namespace. Moved to `ge::classic::content`, each such name must be
qualified (`ge::shooter::Locs`) or the build fails at the first one. Compile every object after the
move, not only the game: `ninja -C build/debug -k 0 $(ninja -t targets all | grep -o '^[^:]*_test\.cpp\.o')`.
