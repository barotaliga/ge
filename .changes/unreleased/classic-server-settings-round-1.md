---
type: Added
title: The classic server's settings under our names, first round
---

A Shooter's Edge server takes more of the classic server's settings, each under our own name with the classic name still working: the download switches are now `sv_download*` (`allow_download*` still sets them) and `sv_downloadchunks`; `sv_country`, `sv_city`, `sv_coords` and `sv_publicaddr` say where the server is in its server info (`countrycode`, `city`, `coords`, `hostport`); `sv_watervis`; `sv_defaultname`, `sv_unfake`, `sv_colorkick` and `sv_infoflood` guard names and chat; `sv_maxspectators` gives spectators their own slots and `sv_fullspectate` seats a player joining a full server as a spectator; `sv_reconnectwait` ignores reconnect storms; `sv_maxping` sends a player whose ping is too high to watch; `sv_minping` holds every player's ping up to a floor, which players may set with `minping` before a match when `sv_minpingcmd` allows.
