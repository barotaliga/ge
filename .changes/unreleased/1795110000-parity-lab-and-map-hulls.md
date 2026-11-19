---
type: Fixed
title: Maps built from a plan load on existing servers; se-parity compares a player's move with one
---

A map built with `secli map build` or `ge map export` wrote each clip hull with its root last, and an existing server refused the map at load. The hulls are now written root first. The new `se-parity` lab plays one scripted player through the parity hall on an existing server and on ours and compares the two paths command by command.
