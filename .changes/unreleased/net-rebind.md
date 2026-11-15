---
type: Added
title: A sealed connection follows its client when a NAT rebinds the port
---

A secure node now keeps a connection when the client's address changes under it: a sealed payload from an unknown address is tried, behind the strangers' guard, against each connected peer's key inside its replay window, and the peer it opens under moves to the new address (`NodeStats::rebound`). A stranger's or a replayed payload moves nothing; a plain connection still reconnects. The `net-chaos` lab takes `--secure=0|1|both` (smoke runs both), and `ge net stress --chaos=... --secure` seals the chaos sessions.
