---
type: Fixed
title: Classic enhance's models and pictures stages work on the sequel's packs
---

Every model of the sequel was written to one file (they are all named `tris` in their own folders) with no skins; the models stage now names each by its path under its kind's folder (`model_name`), reads model skins and sprite frames from their PCX pictures, and the window finds a model's enhanced skins by that name. The pictures stage reads a game with no picture wad (its PCX pictures), and the lightmaps stage reads the palette in the row's own form.
