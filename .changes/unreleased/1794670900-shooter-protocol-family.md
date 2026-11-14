---
type: Changed
title: The classic protocol family's wire, net channel and info strings are the shooter plugin's
---

`plugins/shooter/protocol/` (`ge::shooter::protocol`) now holds what the first classic game's protocol and its sequel's share: the field readers and writers, the coordinate and angle quantization, the CRC-16, the net channel and the info strings. Any shooter game on the engine links them; Shooter's Edge's two protocols keep only their own version, ports, move checksum and usercmd.
