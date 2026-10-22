---
type: Added
title: TCP, big messages, priorities and a send budget in the network
---

`ge::net` carries messages up to 64 KB on a reliable channel that is configured for them
(`ChannelConfig::largest`). Each one is sent in pieces and delivered whole.

Channels have a priority, and a connection can have a send budget (`budget_kbps`).

There is a TCP transport (`open_tcp_host_transport`, `open_tcp_client_transport`) that carries the same
packets as length-prefixed frames, and the simulated network can make a link a stream
(`LinkConditions::stream`).

The `net-link` lab and `ge net sim` take `--transport=tcp` and `--budget`, and a `download` traffic mix.
