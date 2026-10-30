---
type: Added
title: Views aimed at the sun or the moon
---

A Fall Line view name takes `@sun` or `@moon`, with an optional `:<lens degrees>` (`--views=vista@moon:4`), to aim the view's eye at that body at the scene's hour, in every lab that takes the named views; `ge::render::aim_at` does the same for any `FrameView`.
