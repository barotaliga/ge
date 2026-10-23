---
type: Added
title: The army's helicopters
---

Fall Line's army flies helicopters in the hunt, in the new `games/fall_line/sim/army_heli` and `army_land`:

- **The call.** The wanted level's roster sends them in, staggered from the map's edge on his side.
- **The station.** Each machine chooses, and records on the decision trace, the pylon turn round him, the echelon behind a fast skier, or the search rings while he is lost.
- **The landing.** A clear, flat zone near him takes a landing and the section runs out of the doors onto the hunt as soldiers on foot; where there is none, they fast-rope down. Men he has outrun are picked up and set down ahead of him again.
- **Home** when the hunt is over.

The new `army-flight` lab flies five scenarios (open snow, the woods' fast rope, the leapfrog, hiding under a spruce, a fast line). The `army`, `army-sleds` and `wanted` labs and tests stage the hunt without helicopters (`ArmyState::send_helis`), so their digests are unchanged.
