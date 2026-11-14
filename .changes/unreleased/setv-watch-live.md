---
type: Added
title: Live broadcasts of any server on the hub
---

`setv --watch=<server> --web=<tcp port> --master=<master>` joins a server as a spectator and broadcasts its view live on the engine's TV wire, to game windows over UDP and browsers over a WebSocket, listed on the masters as a TV node that names the server it watches. A hub puts that broadcast's Watch button on the server's card (`poller_attach_broadcasts`), so a classic server gets one too. A TV host can be listed under a game's protocol (`TvHost::public_protocol`) and name its server (`TvHost::source`).
