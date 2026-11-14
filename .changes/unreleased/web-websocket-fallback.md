---
type: Added
title: A page without WebTransport plays a hub's server over the forwarder's WebSocket
---

`ge::forward::WsForward` (`forward/websocket.hpp`) is a forward whose clients are WebSocket links. A page opens a link on `/play?to=<server>`, and each binary message is one of the game's datagrams, sent on to the server as UDP. A refusal closes the link with its words as the close's reason. `sefwd --websocket=<port>` runs the WebSocket face beside the WebTransport one and names the port in the forward's status record (`websocket`). The hub's join page hands the page both ways in. The web player tries WebTransport first and falls back to the WebSocket when the browser has no WebTransport or the session does not open. `--join=` now also takes a `ws://` or `wss://` URL. The WebSocket codec moved from `engine/tv` to `engine/net/websocket.hpp`, and `forward::path_destination` reads a page's `to=`.
