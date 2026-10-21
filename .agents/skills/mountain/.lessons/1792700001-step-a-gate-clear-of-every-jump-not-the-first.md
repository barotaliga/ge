---
title: Step a gate off the jumps clear of every one of them, not just the one it landed on
date: 2026-10-21
scope: plugins/mountain/mapgen/speed_events.cpp
concepts: [turn-gate-arcs, jumps, super-g, giant-slalom, sweep]
---

A turning course steps a gate that falls on a jump's lip or landing to that jump's either side.
Where a course keeps several jumps close together (a run of drops down a steep face, seed 23 at
v8 has five on its super-G), the step off one landed the gate on the next one's lip, and only a
30-seed sweep found it. Pick the step among every jump's sides that is clear of all of them, and
fall back to past the jumps. A rule a setter avoids "by stepping off" needs the step checked
against the same rule.
