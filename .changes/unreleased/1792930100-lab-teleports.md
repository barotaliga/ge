---
type: Added
title: The labs find teleports in an actor's track
---

`ge::lab::find_teleports` (`lab/teleport.hpp`) takes a track (each sample's time, position and speed). It returns every step that moved farther than the faster of its two ends' speeds could carry it. `report_teleports` writes the count as a metric and a note for each jump, and fails the lab if asked.

The `ride`, `sled`, `heli` and `para` labs report teleports under `--trace`.
