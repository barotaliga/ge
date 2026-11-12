---
type: Fixed
title: the TV labs read the same loopback bytes on macOS as on Linux
---

`tcp_settle`, `tcp_settle_pairs` and `tcp_waiting` (`engine/net/tcp_socket.hpp`) let a test or a lab that holds both ends of a loopback stream wait until the bytes and the connection have arrived, so the `se-tv` and `se-tv-node` labs read the same bytes on the same step on macOS, where loopback is queued, as on Linux.
