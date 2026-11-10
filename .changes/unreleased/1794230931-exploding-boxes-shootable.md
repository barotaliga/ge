---
type: Fixed
title: The single-player game's exploding boxes can be shot
---

A single-player program's exploding box (its model a brush model in its own file) now gets the file's bounds from `setmodel`, as the single-player game gives them, so shots hit it and it blows with its blast of 160; before it stayed a point and every shot passed through it. Our campaign mod's boxes blow at once, setting off a box beside them in the same blow. Lab `se-client` takes `--fire=<frames>` to draw a view again after the player fires.
