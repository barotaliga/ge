---
title: An image whose pixels change bumps its revision, or the GPU keeps the old texture
date: 2026-11-05
scope: engine/ui/image.hpp, engine/render/ui_pass.cpp, games/shooters_edge/menu/gallery_page.cpp
concepts: [ui-image, texture, revision, upload, address, gallery, thumbnail, stale]
---

The GPU's 2D pass (`render/ui_pass.hpp`) keys each uploaded image by its ADDRESS. Before
`ui::Image::revision` it uploaded an image once and never looked again, so a box whose image's
pixels were refilled in place (a gallery's thumbnail slot showing the next page) drew the old
picture on the GPU while the CPU raster, which reads the pixels every time, looked right: a lab
would never have caught it. Keep such images in slots that never move (a `std::deque`, never a
`std::vector` that reallocates) and bump `revision` each time their pixels change; the pass
uploads again when it moves. A freed image whose address is reused by a new one with the same
revision would still draw stale, so slots, not temporaries.
