---
type: Added
title: two bot behaviours to judge (the press, the way in sight) and people's keys and fight feet split
---

Shooter's Edge's bots gain two switches, off in every preset until judged: `press` (in a fight the forward key is drawn with the dodge's side, on at the enemy or back off, so a bot closes in as people do) and `lookway` (a calm walker looks at the furthest point of its route it can see, never past a teleporter, instead of a point that can lie behind it). The `se-people` lab splits the keys it reads off a recording by whether an enemy is in view (`people.keys.fight.*`, `people.keys.calm.*`) and the fight's feet on dry ground by the distance to the enemy and by a loaded launcher (`people.fight.*`); `se-team` reports how often the press runs (`team.press.*`) and its trace carries the pilot's heading, the calm view's target and the press's note.

The bots now press their keys with a hand on a keyboard by default (`keys` and `keysteer` in every preset); `-keys,-keysteer` gives back the analog axes.
