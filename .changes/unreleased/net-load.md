---
type: Added
title: Load tests many network sessions in one host process
---

`ge::net::run_load` runs many full sessions (a host and up to 32 clients each, the traffic mix's inputs, snapshots and reliable events) in one process on one simulated network and times each host's step by a clock the caller passes. The `net-load` lab charts the step against the number of sessions, and `ge net stress --load=<sessions>` prints it. The simulated network now finds a datagram's socket by an index instead of a list, so a thousand nodes cost what ten did; no session's bytes change.
