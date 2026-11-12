---
type: Added
title: The engine built for the browser, and Shooter's Edge's web viewer
---

A `web` preset builds a game's viewer with Emscripten, drawing through the browser's WebGPU onto a hub page's canvas (`ge_add_web_viewer`, `engine/web`). Shooter's Edge's viewer fetches the content packs a hub serves beside it and a demo by its hash, and plays it. The hub now hands those packs to the viewer and sends answers larger than a connection's backlog whole; it used to drop any file over 256 KiB.
