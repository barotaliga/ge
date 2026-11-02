---
type: Added
title: Screenshots on F12, in PNG, JPEG, AVIF or HEIC
---

`ge::capture::Screenshots` writes the frame a game shows, named by the time, off the game's thread with no frame held (`render::ScreenshotCopy`, one call a frame, copies the frame back through `gpu::ReadbackRing`; `capture::add_screenshot_command` is the console's `screenshot`), in the format the player's `scr_format`, `scr_quality` and `scr_chroma` cvars name (`capture::add_screenshot_cvars`): PNG (`lab::encode_png_rgb`), the engine's own JPEG (`capture/jpeg.hpp`, the same bytes on every platform), AVIF through the engine's AV1 encoder or HEIC through the platform's, boxed by `capture/heif.hpp`, falling back when a build or machine lacks the encoder. Shooter's Edge (`<game folder>/screenshots/`) and Fall Line (the per-user folder's `screenshots/`) bind it to F12 and the console's `screenshot`.
