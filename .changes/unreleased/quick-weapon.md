---
type: Added
title: the quick weapon: a fire key that fires the best weapon carried, then switches back
---

`ge::shooter` gains a quick fire any shooter can wire to its fire keys (`plugins/shooter/quick_weapon.hpp`): pressed, it asks for the first weapon of a priority list the player carries with ammunition and fires it in the same command; let go, it asks for the weapon kept in hand. A tap still fires one shot, an ask a reloading weapon could not take is repeated until it lands, and a weapon the player picks himself wins. It runs on the client from what the HUD knows, so it works on any server. Shooter's Edge wires it to `cl_quickweapon`, `cl_quickweapon_prio`, `cl_quickweapon_default`, a second key `+attack2` with `cl_quickweapon_prio2`, and two looks: `cl_quickweapon_preview` draws the weapon a press would fire see-through in hand (the renderer's `HeldDraw::opacity`, a moving screen door the TAA averages), `cl_quickweapon_hide` hides the hand instead.
