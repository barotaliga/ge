---
type: Added
---
The `shooters_edge` window joins a game over the network: `connect <host>[:port]` plays on a server elsewhere with your keys, mouse, name and colours, drawn as a client demo of it is drawn (the map, the items, the other players, the HUD, the sounds and the printed lines); `disconnect`, `reconnect` and `netstatus` beside it. The new headless `shooters_edge_host` serves the baseline mod on a map from your game folder over UDP (port 27500, or `-port <n>`) for windows on this machine or the network to join.
