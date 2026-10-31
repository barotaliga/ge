---
type: Added
title: Fall Line's snow line follows the day's air, and skiers breathe in the cold
---

Fall Line's snow line is now read off the map's day of air at the sky's hour (`air::snowline_at`, over `wind::snow_level`) unless one is picked by hand, so on a mild day it climbs through the afternoon, turning the fall to rain, and sinks again at night. A sky can pick the day's mean temperature (`SkyPick::mean_c`). Every skier breathes white puffs in the cold (`skier/app/breath.hpp`), as thick as the air at his mouth shows a breath. The map's day of air moved to `air::climate_of`.
