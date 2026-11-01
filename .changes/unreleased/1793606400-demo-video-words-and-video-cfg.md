---
type: Added
title: Demo videos say what they show, and video.cfg sets how they are made
---
A video made with `-demovideo` writes the map, the clock, whose eyes and the score in its upper left, in the packs' own console font when they carry one, and draws the status bar in the packs' pictures. It draws in the newer look (the modern look and every classic-enhance switch at its default) whatever the player plays with, then reads `video.cfg` from the game folder for `demovideo_size` (1080p), `demovideo_fps` (60), `demovideo_codec`, `demovideo_text` and `demovideo_hud`; the command line still wins.
