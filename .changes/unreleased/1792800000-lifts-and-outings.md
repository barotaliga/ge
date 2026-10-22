---
type: Added
title: The lifts run, and the free ride rides them
---

The mountain plugin gains the lifts (`plugins/mountain/lifts/lift_loop.hpp`). Every chair, cabin and T-bar goes round its lift's loop as a pure function of the clock (`carrier_at`, `carrier_place`), so it is never stepped and never stored. A chair or a gondola slows its carriers through its terminals and spaces them evenly in time. The plugin also states each lift's load zone and where it lets a rider go.

Fall Line's free ride rides them (`games/fall_line/sim/lift_ride.hpp`). A skier who comes slowly into a load zone facing up the line waits there (a `boarded` event). The next carrier takes him up, and he is let go at the lift's let-go point (an `unloaded` event). The bot rides an outing (`outing.hpp`, `simulate_outing`): down a piste, up the lift at its foot and down another run. The `free` lab gains an outings table, and the new `lift-ride` lab rides every lift of a seed.
