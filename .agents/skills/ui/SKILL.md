---
name: ui
description: "Use for engine/ui, engine/charset (the bitmap fonts, noise layers and text looks) and Fall Line's HUD and cards in gameengine2: the immediate-mode 2D layer (rects, images, SDF text), the strings table, the HUD readouts and presses, the shell's cards and settings, and the Dear ImGui developer UI."
---

# UI

Fall Line's UI is the cards (title, front door, options, pause, finish) and the HUD (speed,
time, gates, minimap, the body tile, the presses), drawn by an immediate-mode 2D layer on
the GPU. While porting, `porting/subsystems.md` § ui names the reference modules.

## The layer (`engine/ui/`)

- Primitives: rounded rects, images (atlas), lines, and SDF/MSDF text from one baked
  font atlas. Everything is batched into one vertex buffer per frame and drawn last.
- Layout: simple rows and columns with anchors, enough for the game's cards. No CSS
  engine.
- Input: hit-tests on the frame's pointer and touch state, the HUD press and the thumb
  guard (a press is a press only after the thumb settles), and the menu cursor for keys
  and gamepad.

## The charsets (`engine/charset/`, `docs/charsets.md`)

- Ten monospaced ASCII fonts, each committed at 256, 512 and 1024 px as one channel of
  coverage on a 16 x 8 grid of 1:2 cells. A byte's cell is `cell_rect(size, code_of(byte))`,
  and the advance is the cell's width (half its height). `atlas_size_for(cell_h)` picks the
  atlas to shrink from. Codes 1..31 are UI symbols (`Sym`), 10 stays empty, and 127 is the
  missing glyph.
- The fonts are built, not drawn by hand. Seven share one set of skeletons
  (`design_glyphs.cpp`, a small path language) stroked by each font's `Style` (`charset.cpp`'s
  `style_of`). Terminal and matrix are pixel art (`pixel_art.cpp`), and segment is bars. To
  change a glyph, edit its skeleton or its art. Then rewrite the atlases with
  `GE_GOLDEN_WRITE=1 build/<preset>/tests/charset_charset_test`, and show `make lab
  LAB=charsets` before and after.
- No ink may touch a cell's outer pixel ring at 256 px; only the box lines and shades reach the
  edge, so that they tile. `fit` in `pen.cpp` narrows a glyph that is too wide.
- Effects are a `Look` (fill, erode, outline, shadow, glow, bevel, overlay) over the tileable
  layers. Sizes are shares of the cell height, so a look stays the same at any text size.
  `paint_text` is the CPU reference, and `make lab LAB=charset-looks` pictures it. A GPU text
  pass (M27b) must match it.
- Every new code, font or look must stay ASCII, monospaced, and our own drawing. Never trace or
  embed outside font data.

## Fall Line's UI (`games/fall_line/app/ui/`)

- **Every word the player reads** comes from one strings table. No literal text in a
  draw call.
- The shell's surfaces (`splash | menu | loading | pause | run | replay`) and the rule
  that only the pause card stops the simulation.
- Settings, records and ghosts are saved through the platform's save directory
  (`platform` skill), one key each, and a stored blob is merged into the settings this
  build offers in one place (`merge_settings`).

## Dev UI

Dear ImGui, compiled only with `GE_DEV_UI=ON`, for the developer page (the instruments,
the free camera, the REPRO line). It is never on a release path.

Check the UI at the reference viewports: 1280×720, 390×844 and 844×390.

## Labs

`charsets` (every atlas, a specimen of each font in use) and `charset-looks` (the layers,
every look, a mock game screen) are M27a's. M10 lands `screenshots`: the HUD and every card at 1280x720, 390x844 and 844x390, each
captioned with its state and what to look for (overlap, clipping, legibility).
