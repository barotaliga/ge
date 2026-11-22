---
type: Added
title: Web downloads for every classic game, GitHub repositories too
---

Downloads from a web address now work in Quake and QuakeWorld as well as Quake II. Our QuakeWorld server names one with `sv_downloadserver` and our client fetches the skins and extras it lacks from there before the level begins, the game's own connection behind it. The `cl_http_*` settings are shared by every classic game. A web address may be a GitHub repository: `github:<owner>/<repo>[@<branch>][/<folder>]` or the repository's page, read as its raw files (secure addresses are fetched once TLS lands; until then they fall back to the game's connection).
