---
type: Added
title: Join an IPv6 server, and turn every sound off
---

`connect` now accepts an IPv6 address, written as `[2001:db8::7]:27500` or bare, and the client
joins from an IPv6 socket. Quake II's `net_enable_ipv6` 0 keeps its client on IPv4, and its
`net_ip6` names the address that socket uses. Every game with a window also gets `s_enable`,
where 0 turns all sound off.
