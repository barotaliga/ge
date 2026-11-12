---
type: Fixed
title: A grenade's glow is faint and red, from its ends only
---

Shooter's Edge's classic look no longer lights a flying grenade with the rocket's big orange light, which a classic grenade never had. In the enhanced look (`ce_trails`) the grenade's painted ends cast a faint light of their own red, taken from the pack's skin, and that light no longer falls on the grenade's own body. `DynamicLight` gains `entity`, the entity a light rides on, which `dynamic_light_at` leaves unlit.
