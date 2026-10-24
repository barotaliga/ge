---
title: Tests in a file run last-declared first, so a golden must play its scenes itself
date: 2026-10-24
scope: tests/check.hpp, tests/fall_line_army_meet_test.cpp
concepts: [test, golden, order, registration, memo, scene, digest]
---

`tests/check.hpp` runs a file's `TEST`s in reverse order of declaration, so a golden test placed
last that reads what the earlier tests left behind runs FIRST and finds nothing. Make each scene
a function returning what the golden keeps (its state digest), memoised once
(`template <std::string (*Scene)()> const std::string& once()`), and have both the scene's own
test and the golden test call it: either order plays each scene exactly once, and a run filtered
to the golden alone still holds every row.
