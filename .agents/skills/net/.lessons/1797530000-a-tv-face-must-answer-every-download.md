---
title: A TV face must answer every download, in both games
date: 2026-11-14
scope: games/shooters_edge/net/sequel_tv_face.cpp
concepts: [sequel-tv, download, signon, stock-client, older-recording, black-box]
---

A recording names every model the recorded server precached, and a viewer's packs may lack one
(the full game's demo1 names a weapon flash model no pack holds). A stock client asks for it
(`download <path>`) between the configstrings and `begin`, and waits: a face that ignores the
command leaves the viewer on the loading screen with no error. Refuse at once (the sequel's
`svc_download`, 16: size -1, percent 0) and the client says the server lacks the file and goes on.
The classic face needed the same (lesson 1797500000); check any new face for it by watching the
game's own demos in a stock client, not only our recordings, which name nothing missing.
