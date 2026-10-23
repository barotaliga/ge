---
title: A loss burst on a quiet link outlasts a few sparse asks
date: 2026-10-23
scope: engine/net/portmap.cpp, engine/net/conditions.cpp
concepts: [loss, burst, gilbert-elliott, retry, lease, renewal]
---

The net-reach lab's NAT-PMP host lost its lease on the bad preset: the third renewal's four asks
(250 ms doubling, 3.75 s) all went unanswered. The conditions' bursts are Gilbert-Elliott chains
stepped per PACKET on each directed link, not per second, so a link that carries one ask a second
stays in its bad state across seconds: four sparse asks can all fall in one burst. A step with
time to spare (a renewal, with half the lease left) should keep asking until its deadline rather
than count tries; budget retries by the deadline whenever there is one.
