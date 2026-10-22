---
title: A scripted pursuer hidden longer than the search time ends the hunt; size the hidden share first
date: 2026-10-22
scope: tests/fall_line_wanted_test.cpp, games/fall_line/labs/wanted_lab.cpp
concepts: [wanted, scenario, sight-line, golden, lab]
---

A hunt is lost once every pursuer has been out of sight for `wanted::kLoseS[stars]` (12 to 40 s),
so a scripted path that spends half a lap behind a ridge escapes before anything else happens,
and a golden cut on it holds nothing interesting. Size the hidden stretch below the search time
(the ridge test's square hides one 10 s side of a 40 s lap). On a real map look at the `wanted`
lab's sight sheds first: from seed 38's start gate a soldier sees almost nothing below the
convex brow 50 m down, so a post at the start loses him within seconds of the start.
