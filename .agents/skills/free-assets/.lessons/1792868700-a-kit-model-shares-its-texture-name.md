---
title: Every model of a low-poly kit points at a texture file of the same name with different pixels
date: 2026-10-24
scope: games/shooters_edge/content/models
concepts: [glb, texture, colormap, kit, rename, import]
---

The GLB files in each Kenney kit carry no texture: they name `Textures/colormap.png` beside
them, and every kit ships its own `colormap.png` with different colours at the same name. Copy
a model out of its folder, or put models of two kits in one directory, and it draws with the
other kit's palette or with none. Pack the image into the GLB's binary chunk (a bufferView and
a mimeType per image, the uri dropped) before renaming it. The same packs ship a
`Previews/<name>.png` beside each model, which is the quickest way to see a kit without a GPU.
