---
type: Added
title: Shooter's Edge's server host and the headless dedicated server
---

Shooter's Edge has a server host: slots for remote clients and for bots, a bot's commands made and run exactly as a person's through a command source that only reads the server, and an input log of every frame (its connects, its packets and its hash) that replays the run to the bit and names the first frame that leaves it. The headless `shooters_edge_server` runs it with its slots filled by bots (a seeded stand-in until the real bots), prints status lines and the run's digest, writes the log with `--log` and replays one with `--replay`. The test arena has eight spawn points and a roof for a server, and the `se-server` lab runs a full server of bots and replays its log.
