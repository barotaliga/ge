---
title: A braked belt creeps down a slope unless its shear bites like a static grip
date: 2026-10-22
scope: plugins/snowmobile/body.cpp, plugins/snowmobile/spec.hpp
concepts: [parking-brake, belt, shear, slip-ref, static-grip, park-slope, creep]
---

The belt's grip is shear in velocity form: its pull grows with the belt's slip over the snow
and saturates past `grip::kSlipRef` (1.6 m/s). With the belt clamped at zero, the hold a
machine has on a slope is only what that slip buys. Left on a 15 degree groomer, it found a
steady creep of about 1 m/s and slid 9.6 m in 10 s (the `park-slope` scenario). A slope it
should sit on needs a hold that does not wait for a slip.

The fix is in the grip and is marked as the arcade's: a belt held still by the brake
(`tread_speed == 0`, brake past half) bites at `grip::kLockedRef` (0.02) of the slip
reference, so its paddles hold like a static grip. The machine now creeps 0.3 m in 10 s and
settles. Braking at speed is untouched, because there the brake holds the belt at the way less
`kBrakeSlip`, never at zero.

A reading of the lab that would have caught it sooner: watch the `metres` column of a scenario
that should not move, not only its end speed. A creep's speed is tiny, but its metres grow.
