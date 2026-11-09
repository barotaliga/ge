---
title: Take the mouse on the focus, not on a click; and never feed a held pointer to a menu
date: 2026-11-09
scope: engine/input/mouse_capture.hpp
concepts: [mouse-capture, relative-mouse, focus, dock, menu-pointer, release-bits]
---

A window that lets the mouse go on a lost focus and takes it back only on a click in the window
makes the player click twice after the Dock or alt-tab: once to bring the window forward, once
more before the look works. Decide the capture from the frame's facts every frame instead
(`follow_capture`: focused, console, menu, watching, and the game's release bits), so a focus
gained takes it with nothing else. And a menu kept up while the pointer is held must not read
the pointer: it is frozen where it was, so its hover and click land on whatever row sat under it.
