---
type: Added
---
The glTF loader reads a material's base colour texture. Each triangle takes the colour of the texel at its middle (linear, times the base colour factor) as a material of its own, so a model built on a colour atlas (the common free low-poly kits' one palette PNG) draws in its colours in every pass and in the `models` lab, with no texture on the GPU. `assets/image.hpp` is a PNG reader that takes palette images too. The inflater moved from `engine/lab` to `engine/assets`. A texture the loader cannot read (a JPEG, a URI image) keeps the base colour factor and says so in the load's notes.
