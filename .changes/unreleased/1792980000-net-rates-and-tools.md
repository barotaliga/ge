---
type: Added
title: Network rates a client asks for, ge net sweep, ge net watch and fake link conditions
---

A replicating client now asks for its own rates: snapshot bytes a second (`rate`), snapshots a second (`update_rate`) and input messages a second (`cmd_rate`). The server caps them with `max_rate` and `max_update_rate`, and a snapshot fits within what its client takes. `ge net sweep` runs a replicated world once for each value of one knob (a rate, the interpolation, the input backup, the tick or a link condition). It reports the bandwidth each way, the view delay, the corrections and the rewind gaps, and names the cheapest value within reach of the best. Link conditions gain drop patterns (`drop-every=2` loses every other packet). A running game or server can open a live tap (`net/tap.hpp`) that mirrors its packets to a loopback port, and `ge net watch` shows each connection there live and then measures the whole watch. `transport_fake` puts a live game's sends through fake delay, jitter and loss.
