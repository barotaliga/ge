---
type: Fixed
title: Quake II co-op helpers keep to the map's exits and walk where they mean
---

A bot that came into a Quake II map at a spot facing another way could walk half round from where it meant until something else turned it: the server's turn of its view was never taken. Helpers at base2's co-op spots ran backwards into the lift's button and carried the party back to base1. A bot now hands the turned view back as it is, and its helpers know Quake II's exits (a trigger that fires a level change, and a button that lifts into one) and keep off them. Helpers in Quake and in Quake II now share one round of the frame.
