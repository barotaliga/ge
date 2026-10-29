---
title: A flooding client is a connected peer, so leave it out of the honest means
date: 2026-10-29
scope: engine/net/traffic.cpp, engine/net/traffic_attack.cpp, engine/lab/labs/net_link_lab.cpp
concepts: [flood, flooder, rtt, host-means, security, attack, lab]
---

The secure `net-link` run first showed the attacked round trip 50 ms over the plain one on `bad`
and `awful`. The guards were fine: the loud client had a good token, so it was one of the
host's peers, and its starved link was averaged into the host's round trip, loss and bandwidth.
Isolating each attack in turn (one rate on, the rest off) found it in one run. A session's
summary over "every peer" must name which peers are the honest ones (`traffic.cpp`'s `honest`)
once attackers can hold a seat. The global handshake bucket also starved honest joins when a
flood came from one address; flood from many addresses so the per-address guard is what the
lab measures, and keep the global one well over a real join rate.
