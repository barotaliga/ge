---
type: Changed
title: the quick weapon's ghost turns solid and back without a weapon switch
---

With `cl_quickweapon_preview 1`, a tap of +attack no longer shows the weapon kept in hand being put away and raised again: the hand holds the quick weapon the whole time, its see-through ghost easing to solid as it fires, staying solid a moment, and easing back to the ghost while the kept weapon returns unseen. `ge::shooter::quick_look` now steps the fade itself (`kSolidS`, `kHoldS`, `kGhostS`) and says when the weapon shown is not the one in hand (`QuickLook::rest`); Shooter's Edge hands that weapon to the hand before it steps (`quick_show`, `HeldState::show`).
