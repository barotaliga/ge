---
type: Added
title: A server at home opens its port on the router
---

A dedicated server at home can open its game port on the home router with `sv_open_port 1` (and `sv_gateway` to name the router). It asks by PCP, NAT-PMP or UPnP, prints the public address players reach it at (or why no router answered and what to do), renews the forward and takes it down when the server stops.
