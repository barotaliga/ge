---
type: Fixed
title: No crash on a brush solid with no brush model, and campaign bots keep out of killing pits
---

A campaign on the 2021 release of Quake could crash as e4m1 started. The trace read past the end of the server's brush models for an entity that was solid as a brush but had no brush model. That entity is now met as its box.

On Ground Zero's last map, the campaign bot killed the boss and then died in god mode in the map's traps, which kill through any protection. These are two pits at the ends of corridors with currents blowing into them, and two pits with killing floors under turning grinders. The bots now keep off floors in and beside a trigger that kills. They also keep off currents that carry into one, walks and drops across one, and turning wheels. Their guard at a pit's lip now sees these pits as it sees lava. On that map the bot now lives to the end of the run.
