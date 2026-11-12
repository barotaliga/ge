---
type: Added
title: classic players chain through our forwarder
---

Our forwarder now speaks the classic way of setting a route up: a player sets `prx` to the server (or to more forwarders and then the server) and connects to the forwarder, which takes its hop off the route and connects onward with the player's own details. Unlike the classic forwarders it tells the player "connected" only once the server has said yes, and passes a refusal on in the server's words. It answers `ping`, `status` and `pingstatus` (the servers it knows and their pings, for a route finder), at most once a second to any one host, and keeps a player whose router changes their port. Our own client can connect through one with the new `prx` setting in its config.
