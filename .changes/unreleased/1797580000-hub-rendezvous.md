---
type: Added
title: The hub runs the rendezvous too
---

A hub started with `--rendezvous` (port 3478 by default) runs the rendezvous beside the master and the account service: games hosted behind home routers register there, players join them by id, it answers STUN, and it relays a pair whose routers will not open to each other.
