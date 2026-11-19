---
type: Added
title: cl_physfps sets the moves a second sent to a QuakeWorld server
---

`cl_physfps` sets how many moves a second you send a QuakeWorld server (0, the default, as many as it allows), and `cl_physfps_spectator` the same while you watch; each move now carries its exact share of a second, the leftover fraction of a millisecond carried to the next.
