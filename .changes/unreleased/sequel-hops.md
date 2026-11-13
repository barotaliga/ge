---
type: Added
title: The sequel's client through forwarders and the proxy
---

Shooter's Edge's sequel client dials a forward route before its challenge (`via` in `net/sequel_client.hpp`) and plays through a chain of our forwarders or through our proxies, to our sequel server or an unmodified one. The `se-sequel-forward` lab runs it on the simulated network with a lossy path, or against a live server with `--server=` and `--forward=` or `--proxy=`.
