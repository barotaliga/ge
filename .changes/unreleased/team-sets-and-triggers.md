---
type: Added
title: Your own crosshair picture, the team's item sets, team tags, regular-expression triggers, map groups and two network settings
---

`crosshairimage <name>` draws a picture of your own from the game's `crosshairs` folder (TGA, PNG, JPEG or PCX) as the crosshair, sized by `crosshairsize`.

`tp_took`, `tp_pickup` and `tp_point` choose which items count when you take one and which you can point at for team messages. `filter #tag ...` shows only the team messages ending with one of your tags.

`re_trigger <name> <pattern>` runs the alias of that name when a line from the server matches a regular expression; `re_trigger_options` makes it hide the line, stop the later triggers, wait between firings or watch other kinds of line, and `re_trigger_match_0` to `re_trigger_match_9` hold the parts it matched. `tp_msgtriggers 0` turns them all off.

`mapgroup` and `skygroup` let maps share a place file, coloured light, replacement textures and a sky. `cl_timeout` sets how long a silent server is waited for, and with `rcon_password` set, `rcon` takes the whole line as the command.
