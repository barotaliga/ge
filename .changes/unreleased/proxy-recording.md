---
type: Added
title: recording on the proxy
---

Either proxy can now record every game datagram it passes on, both ways, as a replay file (`engine/proxy/record.hpp`): `ProxyConfig::tap` sees each datagram with its session and direction, `proxy_record` keeps them in two message tracks, and `proxy_read_recording` gives them back in order for a game to play.
