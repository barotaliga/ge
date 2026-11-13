---
type: Added
title: A web page can join a game server over WebTransport
---

`net::open_pipe_transport` gives a transport to one peer whose datagrams the caller carries. `ge::web` adds the browser's WebTransport sessions: `wt_open` (the certificate trusted by its SHA-256), `wt_send`, `wt_take` and `wt_close`, and `wt_pipe`, which makes a session into such a pipe. The viewer reads `--join=https://<endpoint>/play?to=<server>` and `--cert=<hash>`, and Shooter's Edge's web player uses them to join a real server from the hub's page, its client running over the session as it would over a socket.
