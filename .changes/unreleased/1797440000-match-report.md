---
type: Added
title: A finished match leaves a report
---

`engine/net/report.hpp` is the record every game leaves when a match ends (the match, its teams, its players with frags, deaths, suicides, team kills and seconds played, and the game's own columns), written as bytes and read back with damage refused. A game hands it to the server core through `ServerGame::match_report`, which adds the server's part and keeps the last 16 on `Server::reports`. Shooter's Edge's server tallies every deathmatch off its rules and reports it at its frag or time limit; a lobby's warmup is not counted. The `match-report` lab plays bot matches and draws each report as a scoreboard.
