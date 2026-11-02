---
type: Changed
title: classic-enhance reads its files through a content profile
---

Classic-enhance now finds every file it reads (the packs in an install, the maps, sounds, models, sprites, pictures, palette and picture wad) through a content profile, one row of data a classic game (`plugins/classicenhance/profile.hpp`), instead of paths written into each stage. The first game is the first row and reads exactly as before; a second classic game becomes a second row. `ge tool classicenhance -- verify` names the profile it found.
