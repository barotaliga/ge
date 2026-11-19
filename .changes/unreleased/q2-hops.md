---
type: Added
title: Quake II's client through forwarders and the proxy
---

Shooter's Edge's Quake II client dials a forward route before its challenge (`via` in `net/q2_client.hpp`) and plays through a chain of our forwarders or through our proxies, to our Quake II server or an unmodified one. The `se-q2-forward` lab runs it on the simulated network with a lossy path, or against a live server with `--server=` and `--forward=` or `--proxy=`.
