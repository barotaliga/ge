---
type: Added
title: Classic enhance enhances Quake II's wall textures
---

The textures stage reads a map that keeps its pictures outside by each drawn texture's own wall texture file, so Quake II's packs get enhanced textures (the demo's: 474, none before) and `ce_textures` draws them. A texture's material comes from the map's sky and liquid marks and the content row's own material words (`Profile::words`), never from a `*` or `sky` in its name.
