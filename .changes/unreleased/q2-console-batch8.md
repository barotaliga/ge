---
type: Added
title: Quake II's downloads over HTTP
---

A Quake II server that names a web address (`sv_downloadserver`) now has its files fetched from there, several at once, before the level begins; your own repository (`cl_http_default_url`) is asked when the server names none. The server's file lists are followed, a file the web lacks is left lacking, and a web server that fails hands the rest back to the game's own connection. Each file is said as it arrives ("[HTTP] maps/q2dm1.bsp [1.2 MB, 340 kB/sec] [3 remaining files]"), and so are the files that come over the connection ("[UDP] ... [DONE]"). New settings: `cl_http_downloads`, `cl_http_filelists`, `cl_http_max_connections`, `cl_http_proxy`, `cl_http_default_url`, `cl_http_insecure`, `cl_http_debug`, `cl_http_blocking_timeout`. JOIN A GAME's `sb_masters` takes a web list of servers (`http://...` as text, `+http://...` as 6-byte records).
