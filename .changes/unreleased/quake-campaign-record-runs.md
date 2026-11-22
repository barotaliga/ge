---
type: Added
title: Recorded fast runs of Quake's maps read as routes
---

A new lab, `se-q1-speedruns`, reads a folder of recorded single-player runs (plain or packed
demos) and draws each one on the live map: the route, where every kill and secret counted, the
rocket and grenade jumps, and the time to the exit; it also writes every run as rows of data, for
the campaign bot to learn its route order and tricks from.
