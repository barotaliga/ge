---
type: Added
title: fs_artifacts_basedir keeps saves, screenshots and recordings in the game's install
---

Each game's saves, screenshots and recordings stay under that game in the data folder (`q1/id1`, `q2/baseq2`), and Shooter's Edge's own game now keeps its own in `se/` there instead of the folder the game was started in. `fs_artifacts_basedir 1` puts them under the same game folder in the game's install (`fs_basedir_q1`, `fs_basedir_q2`), so a copy of the game beside the program carries them; `playdemo`, the demos page and `secli demos` look there too.
