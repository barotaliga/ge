---
title: The screenshot gallery's logic is the engine's; a game writes only its page
date: 2026-11-05
scope: engine/capture/gallery.hpp, games/shooters_edge/menu/gallery_page.hpp
concepts: [gallery, screenshot, thumbnail, viewer, delete, worker, poll, settle, lab-folder]
---

`capture::Gallery` lists, pages, takes the presses (grid, viewer, confirm), loads off its own
thread and deletes; a game's page maps its keys to `GalleryInput`, calls `set_grid` from its
layout before `input`, `poll`s once a frame and copies a picture into its texture only when its
`stamp` moves. A lab or test calls `settle` after a draw (the draw sets the grid, so the page's
thumbnails are the ones asked for). A lab that draws the folder's path must use a RELATIVE folder:
a temporary directory's path differs by machine and would move the digest on every CI row.
