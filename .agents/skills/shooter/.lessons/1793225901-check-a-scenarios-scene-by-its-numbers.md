---
title: A recorded scenario may sit under the wrong scene; check where it lands against the brushes
date: 2026-10-28
scope: tests/golden/shooter_recording.txt
concepts: [recording, scene, scenario, parity, slidefix, fall-through]
---

`ramp_down_slidefix` followed the 10 degree ramp's scene in the file, and our player fell through
the floor at the 30 degree ramp's height. The recording's landing height (147.80 + 24 + 1/32)
named the scene it really ran on; moved under that scene, all 120 lines agreed exactly. When a
scenario fails from one line on while its twins pass, compare its first contact's height with
each scene's floors before debugging the move.
