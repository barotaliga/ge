---
type: Added
title: The classic 2D art enhanced
---

classic-enhance's pictures stage also writes every status bar picture, the glyph sheet and the menu pictures upscaled by the textures' upscaler with their see-through texels kept (`enhance_picture`, `pictures/enhanced/`, read back by `cached_picture`), and Shooter's Edge's status bar, classic menus, glyph text and demo video draw them at their classic sizes from four times the pixels under the new `ce_pictures` switch (on by default); `ui::scaled_nearest` scales pixel art up by a whole number.
