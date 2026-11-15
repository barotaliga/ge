---
title: A load test on the simulated network times the simulator too; check the step stays flat
date: 2026-11-15
scope: engine/net/stress_load.cpp, engine/net/sim_network.cpp, engine/lab/labs/net_load_lab.cpp
concepts: [load, host-process, step-time, sim-network, socket-lookup, measured, clock]
---

A host's step on the simulated network includes the network's own work: every send and receive
finds a socket. With the sockets in a list that cost grew with the nodes, and the `net-load` lab
read it as the host slowing down beside other sessions (0.41 ms alone, 1.26 ms with 31 others).
When a per-session cost climbs with the session count although the sessions share nothing, look
at the simulator before the node: the fix was an index by node and port (`socket_at`), after
which the step is flat. Keep the clock out of `engine/net` (`LoadPlan::clock_ns` is the caller's)
and fold only counts into the digest; a seed changes the counts only on a lossy link, so a test
that a seed moves the digest needs a preset with loss.
