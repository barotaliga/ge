---
title: Picture thrown things by aiming at where they came to lie, not by a camera fixed beside the player
date: 2026-11-23
scope: games/shooters_edge/labs
concepts: [labs, spent-shells, camera, se-weapon-fx, se-q2-weapon-fx, ce-shells]
---

A fixed "over the shoulder" camera showed the chain gun's brass as specks two metres off, and the
super shotgun's hulls (dropped behind the player as it breaks) not at all. The labs now place the
camera half a metre above the middle of the live pieces, on the player's side, and turn it down at
them (`aim_at_cases` in `se-weapon-fx`, the same in `se-q2-weapon-fx`): every gun's cases fill the
picture wherever they fell. Measure the count (`<look>.cases`) beside the picture: a picture of an
empty floor and a picture that missed look the same.
