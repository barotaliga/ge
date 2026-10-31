---
type: Added
---
Shooter's Edge's own QC mod now builds two variants from shared sources: `qc/arena/`, the deathmatch (unchanged in play), and `qc/campaign/`, the first part of a free single-player campaign on the classic maps: the start and coop starts, the inventory carried from map to map, exits with an intermission, secrets, counters, relays, skill triggers, episode and boss gates, runes and keys, and a death restarting the map. `ge qc build games/shooters_edge/qc/campaign/progs.src` builds it, and `se-campaign --progs=` plays it on a pack's maps. Monsters come next.
