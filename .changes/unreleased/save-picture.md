---
type: Added
title: A picture in every save
---

A save can carry a small picture of the run (the `PICT` chunk, kept as QOI and written first after the meta): `save::put_picture` and `read_picture`, the QOI encoder and decoder, and `play::save_run_pictured` (`ge save write --picture`), which shoots the game's CPU view. The save store keeps each picture's place in its index, so `Store::picture` shows a slot's picture by reading only the front of its file. The `save-picture` lab.
