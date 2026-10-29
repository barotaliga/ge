---
title: Every message the list depends on is acknowledged, and the forward outlives the unlist
date: 2026-10-29
scope: engine/net/hosting.cpp, engine/net/directory.cpp, engine/net/hosting_trial.cpp
concepts: [advertise, unlist, listing, page, ack, loss-burst, portmap, rendezvous, lab-strictness]
---

The first `net-host` runs on `bad` left games out of the browser: an advertise sent once (no
answer, "nothing to amplify") lost to a burst stayed lost until the next refresh five seconds on,
and a list page asked every 500 ms was lost eight times running on a link that carries nothing
else (a Gilbert-Elliott burst is counted in packets, so sparse asks stretch it into seconds). The
fixes: the rendezvous acknowledges each advertise with a smaller answer and the host sends it
again every `ask_us` until it is; the browser asks every missing page at once and again every
250 ms; the LAN probe goes three times. Then a UPnP host's unlist never landed: `hosting_stop`
closed the router's forward in the same breath, so the retries left from the NAT's own port and
the rendezvous, which takes an advertise only from the address the session registered from,
refused them. Take a forward down only after what depends on its address is acknowledged. And a
lab over a scene of a dozen hosts should fail what must never happen (an invite-only game listed)
on every preset, but what loss may delay (a host listed late) only on the presets that lose
little: sweep a few seeds before trusting a pass.
