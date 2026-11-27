---
type: Changed
title: Tab in the console completes to what the matches share
---

In every game's console (except under Quake II's own Tab), Tab with several matching commands or settings now completes the word to the part they all start with and keeps the list of suggestions open: `r_part` and Tab gives `r_particle` while `r_particles` and `r_particle_scale` both match. When there is nothing more to add, Tab selects the first row, the arrows walk the list and Tab or Enter takes the row. A single match is still taken at once. The dim hint after the cursor shows what Tab will add.
