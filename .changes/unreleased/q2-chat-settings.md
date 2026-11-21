---
type: Added
title: Quake II's chat settings, the chat lines on screen, condump and the console clock
---

Chat lines can stay on screen at the lower left (`scr_chathud`, `scr_chathud_lines`,
`scr_chathud_time`, `clearchathud`) and keep out of the message feed (`cl_chat_notify`);
`cl_chat_filter` cleans them, `followip` joins an address a chat line named, and `trigger` runs a
command when a server message matches. Quake II gets its own `cl_chat_sound` and `con_auto_chat`.
The engine's console says a typed line to the team with `con_chat 3`, lets a game's own setting
rule over it, writes its lines to a file (`condump`) and can show a clock (`con_clock`); a HUD
feed can be resized and kept from fading (`Feed::configure`).
