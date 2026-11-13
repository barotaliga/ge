---
type: Added
title: A web page can join a game server over WebTransport
---

`net::open_pipe_transport` gives a transport to one peer whose datagrams the caller carries. `ge::web` adds the browser's WebTransport sessions: `wt_open` (the certificate trusted by its SHA-256), `wt_send`, `wt_take` and `wt_close`, and `wt_pipe`, which makes a session into such a pipe. The viewer reads `--join=https://<endpoint>/play?to=<server>` and `--cert=<hash>`, and Shooter's Edge's web player uses them to join a real server from the hub's page, its client running over the session as it would over a socket.

`ge::webtransport::WebForward` (`webtransport/forward.hpp`) is the forwarder's WebTransport face: a page's session on `/play?to=<host:port>` becomes a client of an `engine/forward` forward and its datagrams go on to that server as UDP, under the forward's allow list. `shooters_edge_hops forward --webtransport=<port>` runs it and prints the certificate's hash; a session's close now carries its reason to the other end.
