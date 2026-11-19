---
type: Added
title: Shooter's Edge predicts the player's moves on a server of Quake II
---

On a server of Quake II joined from the window, the player's own moves run ahead of the server through the same move the server runs, so walking, jumping and turning answer at once instead of a round trip later; a wrong guess is smoothed away rather than snapped, and `cl_nopred 1` draws the server's place. The jump now works on stock servers of Quake II too.
