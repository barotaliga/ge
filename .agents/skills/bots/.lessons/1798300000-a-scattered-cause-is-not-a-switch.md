---
title: Count the deaths by cause and place, and against people's, before building a fix for each
date: 2026-11-19
scope: plugins/shooterbots/brink.cpp, plugins/shooterbots/keyboard.cpp, plugins/shooterbots/nav.cpp, plugins/shooterbots/pilot_gaps.cpp, games/shooters_edge/labs/team_set.cpp
concepts: [lava-deaths, brink, lip, gap-tricks, deaths-tsv, yardstick, obituaries, features-all, judging]
---

dm2's lava deaths (7.9 a match) were read death by death and four real faults found: the brink's
straight sweep stopped by a post where the move slides, the keyboard's lag outrunning the guard's
look, grid nodes hanging 14 units over a lip, a gap proof taking a landing caught on the lip. Each
was built as a switch and judged on the same eight 20-minute seeds: none moved the total beyond
the noise (7.9 -> 9.5, 8.3, 12.8, 7.9), because the deaths spread over five causes and each fix
reaches two or three a set; one (`lipnodes`) moved routes and set off the floor trap four times as
often. People's own yardstick, read afterwards off the lava obituaries in their recordings
(`strings` on the .mvd), was ~5.3 a match: the gap was small all along. Before building a fix for
a death, count the deaths by cause and place across a set (se-team's `deaths.tsv`, a set's too)
and set the total against people's; build only for a cause big enough to move it. And write a
variant's switch as `+x` on the default: `--features=all,+x` also turns on `eye`, which the
preset keeps off (frags 12.8 -> 11.4 on the same seeds), and every such variant reads worse.
