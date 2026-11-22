---
type: Added
title: Web downloads for every classic game, GitHub repositories too
---

Downloads from a web address now work in Quake and QuakeWorld as well as Quake II. Our QuakeWorld server names one with `sv_downloadserver` and our client fetches the skins and extras it lacks from there before the level begins, the game's own connection behind it. The `cl_http_*` settings are shared by every classic game. A web address may be a GitHub repository: `github:<owner>/<repo>[@<branch>][/<folder>]` or the repository's page, read as its raw files. Secure (`https://`) addresses are now fetched too, over TLS with the system's trusted certificates (Mbed TLS, built in); `cl_http_insecure 1` skips the certificate check, and a secure address through `cl_http_proxy` goes through the proxy's tunnel.
