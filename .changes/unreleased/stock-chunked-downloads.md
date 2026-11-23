---
type: Fixed
title: Map downloads from stock QuakeWorld servers are fast, quiet and show their progress
---
A stock server sends most of a download's chunks out of band as a print. The client now
keeps them: it no longer prints a "\chunk" line for each one, and a download no longer
waits on the one chunk a packet carried. The client asks for 30 chunks a packet and drops
chunks left over from an earlier download. While a server is joined, the console covers the
screen. Its foot shows the file's name, the charset's bar and the percentage done. The level
waits for its map, so a game joined no longer prints "playdemo: demo1 stopped".
