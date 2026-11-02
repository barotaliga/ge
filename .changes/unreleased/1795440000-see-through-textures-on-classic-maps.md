---
type: Added
title: See-through textures on classic maps, and PNG and TGA stand-ins
---

A classic map's faces are cut where their texture is see-through: a PNG or TGA in a game's files stands in for a map texture by its name (its alpha cutting the face), a fence texture's index 255 is a hole, and `engine/assets/tga.hpp` reads TGA pictures (true colour, grey, colour-mapped, packed or not) with their alpha.
