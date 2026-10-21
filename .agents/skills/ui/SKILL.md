---
name: ui
description: "Use for engine/ui and Fall Line's HUD and cards in gameengine2: the immediate-mode 2D layer (rects, images, SDF text), the strings table, the HUD readouts and presses, the shell's cards and settings, and the Dear ImGui developer UI."
---

# UI

game5's UI is Preact DOM: the cards (title, front door, options, pause, finish) and the
HUD (speed, time, gates, minimap, the body tile, the presses). The port is an
immediate-mode 2D layer on the GPU.

## The layer (`engine/ui/`)

- Primitives: rounded rects, images (atlas), lines, and SDF/MSDF text from one baked
  font atlas. Everything is batched into one vertex buffer per frame and drawn last.
- Layout: simple rows and columns with anchors, enough for game5's cards. No CSS
  engine.
- Input: hit-tests on the frame's pointer and touch state, the HUD press and thumb
  guard semantics ported from the framework's `input/hud-press` and `thumb-guard`, and
  the menu cursor (`input/menu-cursor`) for keys and gamepad.

## Fall Line's UI (`games/fall_line/app/ui/`)

- **Every word the player reads** comes from one strings table (port of game5's
  `strings*.ts`). No literal text in a draw call.
- The shell's surfaces (`splash | menu | loading | pause | run | replay`) and the rule
  that only the pause card stops the simulation, as in game5's `shell.ts`.
- Settings, records and ghosts are saved through the platform's save directory
  (`platform` skill), with game5's keys and its merge rules (`mergeSettings`).

## Dev UI

Dear ImGui, compiled only with `GE_DEV_UI=ON`, for game5's developer page (the
instruments, the free camera, the REPRO line). It is never on a release path.

Check the UI at game5's reference viewports: 1280×720, 390×844 and 844×390.
