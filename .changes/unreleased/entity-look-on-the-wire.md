---
type: Added
title: Shooter's Edge's server sends a mod's model scale and colour tint
---

Our server now offers the scale and colour-mod protocol extensions and sends a QC mod's `scale` and `colormod` to the clients that take them, so other players see the same sized and tinted models the mod sets. A player's size and see-through share travel too (never to that player himself), and a static entity keeps the mod's own see-through and tint fields.
