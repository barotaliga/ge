---
type: Added
title: Quake II servers listen on IPv6, send packed files compressed and fly spectators as their clients expect
---

A dedicated server listens on IPv6 beside IPv4 when `net_enable_ipv6` is 2 (on `net_ip6` and the same port). Newer Quake II clients get files kept compressed in a `.pkz` without the server unpacking them, a game library that cannot be saved safely is no longer saved unless `sys_allow_unsafe_savegames` was set at start, and spectators on newer clients fly at the speed their own client predicts.
