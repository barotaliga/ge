---
type: Added
---
Classic maps read their texture sequences (`plugins/brush/anim.hpp`): `+` cycles and their alternates turn at five frames a second, and `-` random sets give each surface one frame. BSPX lumps are kept by name, with `RGBLIGHTING` read as the map's colour lighting. `ge tool brush -- info|ents|check` answers a map's counts, its entities and its sequences as JSON, and checks every map on a search path.
