---
type: Added
title: Fast map downloads over HTTPS from the server itself
---

A dedicated server can now send players the maps and files they lack over HTTPS from a port beside its game port: set `sv_http_port`. With nothing else set up it makes its own certificate and tells its players how to trust it, so the transfer is encrypted and checked from the first start; `sv_http_cert`, `sv_http_key` and `sv_http_url` give it a public certificate and host name for other clients too. Speed caps: `sv_http_rate_player` (5000 kB/s by default, a player's parallel downloads together) and `sv_http_rate` (20000 kB/s for everyone, shared fairly among the players downloading). Clients resume a download cut short, check every file against the SHA-256 the server names before keeping it, cap their own speed with `cl_http_max_rate` and refuse files over `cl_http_max_size`. QuakeWorld clients now fetch the map itself over HTTP too, and read a server's `sv_dlURL` as well as `*dlserver`. A 632 kB map that takes 25 s over the classic download takes well under a second.
