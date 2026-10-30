---
type: Added
---
Classic maps now carry their collision and their hearing. The brush plugin keeps a version 29 or BSP2 map's clip hulls (`Map::clip_nodes`, each model's hull heads, `hull_contents`) and refuses a tree or hull that loops. It builds the potentially hearable set from the visibility (`build_phs`, `cluster_hears`). Its fixture maps carry real player and large-box hulls. `plugins/shooter/bsp_world.hpp` turns a read map into the server's brush models, so traces and the player's move run on real maps. Shooter's Edge's `server/bsp_map.hpp` loads a map file for the mod server, and its PVS and PHS answer the server's visibility hook. The `bsp` lab draws a slice through each hull and what a cluster hears.
