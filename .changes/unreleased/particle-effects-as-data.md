---
type: Added
title: Particle effects written as text, and Shooter's Edge's classic effects overridable by a mod's files
---

A new engine module, `ge::particles` (`engine/particles/effect.hpp`), reads particle effects written as a few lines of text (bursts and trails: shape, speeds, life, size, fall, colour or palette run, shine or smoke, cooling, growth, streaks, bounce, a light) and throws them into the render pool on a seed. A bad file is refused with its file, line and the fix. Shooter's Edge reads `particles/*.fx` from the game's files and throws an effect named `explosion`, `gunshot`, `spike`, `blood`, `teleport`, `lava`, `rocket_trail` or `grenade_trail` in place of the classic one. New lab `gpu-particle-effects`; `se-timedemo --particles=sample`.
