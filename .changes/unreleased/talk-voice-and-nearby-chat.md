---
type: Added
title: chat and voice to everyone, your team or the players near you
---

Multiplayer games opt into chat and voice with `engine/console/talk.hpp`; a single-player game no longer has chat commands. In Shooter's Edge (both classic games) T, Y and U open a chat line to everyone, your team or the players near you (`messagemode`, `messagemode2`, `messagemode3`; `say_near` in the console), and V, B and N talk while held (`+voice`, `+voice_team`, `+voice_near`). Nearby means within the server's `sv_earshot` (1000 units) with no wall in between. Chat typed in the game window now actually reaches the server; before, it stayed on your own screen. The server takes the classic voice commands (`voicetarg`, `unmuteall`, `muteall`, `vignore`), so a classic client can choose who hears it too. `say_prox` is now `say_near`.
