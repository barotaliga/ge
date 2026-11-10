---
type: Changed
title: The lightning gun's bolt is one thin, bright look that lights the room
---

Shooter's Edge's enhanced bolt is now one look every player sees (`r_beam_style` and its four styles are gone): as thin as the classic bolt, a hair-thin white-hot core in a narrow blue sheath, kinked every 12 cm and struck afresh 30 times a second. Its brightness is keyed to the eye (`BeamLook::core_keys`, `glow_keys`), so it no longer swells into a screen-filling glow in a dark room at night; sparks and a burn's ember are capped to the eye the same way. It is the brightest light in the room: its lights at the gun, along it and where it lands reach 7 m at 8 times the other effects' share (`DynamicLight::boost`), and its scorches now appear.
