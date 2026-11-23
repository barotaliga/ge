---
title: A box that shows an image needs its uv set to the image
date: 2026-11-23
scope: engine/ui/draw.hpp
concepts: [box, image, uv, player-page, live-picture, revision]
---

`ui::Box::uv` is the source rectangle in the image's pixels and defaults to empty, so a box with
only `image` set draws nothing of the picture: the setup page's live figure first came out blank
in the lab while every unit test passed, because the tests checked the picture, not the draw list.
Set `uv` to `{0, 0, width, height}` (or use a helper that does, as `ClassicPen::picture`), and look
at one lab picture before trusting a new drawing. A picture redrawn in place each frame (the
figure on its turntable) bumps `revision`, so a GPU pass that keyed the upload by its address uploads
it again.
