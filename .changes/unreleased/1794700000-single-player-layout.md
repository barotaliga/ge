---
type: Added
title: Single-player programs on their own layout table
---

`plugins/progs` describes each program form as a layout table (`Layout`: the multiplayer form, crc 54730, and the single-player form, crc 5927), picks the form by a file's crc (`LoadOptions::forms`, `every_form`), and the Shooter's Edge mod server reads every engine global and field through the program's table, so a single-player program runs with its own builtins: prints without a level, autoaim under `sv_aim`, no multicast; lab `se-campaign`.
