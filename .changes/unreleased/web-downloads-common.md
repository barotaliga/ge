---
type: Added
title: Web downloads for every classic game, GitHub repositories too
---

Downloads from a web address now work in Quake and QuakeWorld as well as Quake II. Our QuakeWorld server names one with `sv_downloadserver` and our client fetches the skins and extras it lacks from there before the level begins, the game's own connection behind it. The `cl_http_*` settings are shared by every classic game. A web address may be a GitHub repository: `github:<owner>/<repo>[@<branch>][/<folder>]` or the repository's page, read as its raw files. The game folder being played is added to every path, so one address serves every mod (`github:owner/repo/quakeworld` fetches from `quakeworld/qw/` or `quakeworld/ktx/`); a repository laid out otherwise sets the place by hand with `{game}` where the game folder goes, or `#` at the end of an address that already holds the game's files. Secure (`https://`) addresses are now fetched too, over TLS with the system's trusted certificates (Mbed TLS, built in); `cl_http_insecure 1` skips the certificate check, and a secure address through `cl_http_proxy` goes through the proxy's tunnel.
