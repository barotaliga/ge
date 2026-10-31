---
title: A loss burst is counted in packets, so sparse asks stretch it into seconds; retry to a deadline and seed each cell apart
date: 2026-10-23
scope: engine/net/conditions.cpp, engine/net/portmap.cpp, engine/net/reach.cpp, engine/net/hosting.cpp, engine/net/directory.cpp, engine/lab/labs/net_reach_lab.cpp
concepts: [loss, burst, gilbert-elliott, retry, lease, advertise, unlist, listing, ack, portmap, seed, lab-strictness]
---

- The conditions' bursts are Gilbert-Elliott chains stepped per PACKET on each directed link, so a
  link carrying one ask a second stays bad for seconds: a NAT-PMP renewal's four asks, a once-sent
  advertise and a page asked every 500 ms were all lost in one burst. Acknowledge every message a
  listing depends on and resend until acked; ask every missing page at once, again every 250 ms;
  budget retries by the deadline (half the lease left), never by a count.
- A client keeps asking past `unknown-session` until it gives up: the host's register may be on
  its way.
- Take a forward down only after what depends on its address is acknowledged: a UPnP host's unlist
  retried from the NAT's own port once `hosting_stop` closed the forward, and the rendezvous refused it.
- A lab grid built off one seed repeats one unlucky draw in every cell (25 of 25 pairs failed):
  seed each cell apart (`seed * 31 + cell`). Fail what must never happen on every preset, and what
  loss may only delay on the presets that lose little; sweep a few seeds before trusting a pass.
