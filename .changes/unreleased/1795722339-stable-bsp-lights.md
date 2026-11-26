---
type: Fixed
title: A map's lights hold still as the player walks
---

The modern look drew the 160 nearest of a map's lights and shadowed 48, so in a place that sees far (dm2's big room, a long corridor) the rest shone through the walls unshadowed and which ones did changed with every step. Every light whose reach can touch the view (its cone and the eye's visible set) is now drawn, each of the map's lights casts (96 shadow cubes kept), a light waiting for its cube is dark and fades in, and one giving its cube to a nearer light fades out. The `bsp-light-set` lab walks a map and counts the pops: dm2 from 49 a 100 m to none.
