---
type: Added
title: the route finder picks the fastest way to a server through forwarders
---

`engine/forward/routes.hpp` works out the lowest-ping way to a server from what a server browser measured: straight, or through one or more forwarders, using each forwarder's own list of the servers it reaches. Any game hands it its forwarders' protocol; Shooter's Edge reads the classic forwarders' `pingstatus`. `shooters_edge_cli route <address>` answers the route and the chain to connect with.
