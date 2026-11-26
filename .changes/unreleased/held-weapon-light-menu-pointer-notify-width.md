---
type: Fixed
title: The weapon in hand no longer glares white by a lamp, the main menu frees the mouse, and the top lines use the whole width
---

Shooter's Edge's weapon in hand is lit no brighter than twice the room's own light, so a torch or a lamp beside the player brightens its lit side without washing it out to white (`viewmodel::tone_held`, in every game the engine draws a held weapon for); the classic look's flat light is as it was. The main menu now lets the mouse go as the console does, its pointer hovering and clicking (`in_mouse_release` defaults to 7; 5 keeps the old way). The pickup and message lines at the top of the screen span its whole width instead of wrapping at a third of it.
