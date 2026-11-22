---
type: Added
title: Quake II's server offers downloads by kind and size, names a web copy and cuts long rcon answers
---

Quake II's server makes an asked file's name plain before judging it, offers textures and skies (`sv_download_textures`), pictures (`sv_download_pics`) and any other folder (`sv_download_others`, off by default), refuses files past `sv_max_download_size`, tells a bad resume "File size differs from server.", and sends a downloading client reliable data alone. Its connect answer names `sv_downloadserver` and the map; a long rcon answer arrives in packets of whole lines.
