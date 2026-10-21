---
name: ui
description: "Use for engine/ui and Fall Line's HUD and cards in gameengine2: the immediate-mode 2D layer (rects, images, SDF text), the strings table, the HUD readouts and presses, the shell's cards and settings, and the Dear ImGui developer UI."
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
