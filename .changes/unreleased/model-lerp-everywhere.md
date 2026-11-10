---
type: Changed
title: Smooth view, demo monsters and recorded weapon animations
---

Your own view in Shooter's Edge is now drawn between the server's frames like the models, so turning and moving no longer judder when the server's rate and the screen's differ (`r_lerpmove`). In played demos a walking monster glides through each stride instead of hopping, and in demos and on joined servers a classic game's held weapon plays the animation frames the server sends, blended between them (`r_lerpmodels`).
