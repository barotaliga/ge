---
title: The leader is whoever the links seated first, not the first to join
date: 2026-10-22
scope: engine/net/session_host.cpp, engine/lab/labs/net_session_lab.cpp
concepts: [session, leader, jitter, lab-script]
---

The net-session lab first sent its kick, start and return from client 0, the first to call join.
On lan that worked; on wifi and bad the kick and both phase changes were refused `not_leader`,
because jitter and handshake retries let client 1's hello reach the host first, and the first
SEATED player leads a dedicated server. A script now asks the clients' own mirrors who leads
(`players[me].leader`). Any test or bot acting "as the leader" over a lossy link must do the
same.
