---
type: Fixed
title: Quake II and Quake no longer look dark and murky
---

The classic look shows each wall's painted colour times its light as the games worked it out (measured pixel by pixel against Quake II's own rule: 0.98 to 1.00 of its brightness on four maps, from 0.72 to 1.28), with no exposure or tone curve; neither look's tone curve sinks the dark textures' greys and blues to brown any more. Quake II's classic look is in full colour, as its renderer drew, with no glowing specks on its walls, and its `intensity`, `gl_modulate` and `gl_brightness` settings light its maps.
