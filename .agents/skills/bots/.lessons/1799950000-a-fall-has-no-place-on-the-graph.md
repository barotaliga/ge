---
title: A bot falling past every floor has no place on the graph, and a lever is a door
date: 2026-11-28
scope: plugins/shooterbots/nav.cpp, plugins/shooterbots/way_hands.cpp, plugins/shooterbots/entities.cpp, plugins/shooterbots/buttons.cpp
concepts: [nav-nearest, mover-top, fall, pit-lip, way-edge, pathtarget, lever, func-door-rotating, space, mine4]
---

- `nav_nearest` looked at the ridden movers' top nodes by height alone: falling through space's
  open shaft, with no floor node within 64 of him, the dish's top 2700 units away at his height
  was taken for his place, the route from it steered him off the ledge he was falling onto and
  into the killing floor. A mover's top is his place only within 256 across.
- `way_edge` let a step off a ledge be when the floor under it had no node at all (`nav_nearest`
  -1 read as "not a trap"): a landing with no node near it is no floor with a way back either,
  unless his own route goes down there. Test it with a bench server and a hand-built graph
  (tests/shooterbots_way_edge_test.cpp).
- A `path_corner`'s `pathtarget` fires its name when a train reaches it: a trigger only a train's
  path names is fired, never a step to touch (space's dish pit trigger, walked into from the
  dish's stop tops). `name_fired` holds the three keys that fire a name.
- A turning door with no name and a target is a lever, opened by a touch, and is pressed like a
  button (`door_is_lever`; mine4's lever starts the carts). Its press shows in its angles, not
  its origin. A sliding door's target is most often its area portal: left out.
- Before calling a map a regression of your change, run it on main's binary: base1 had stopped
  finishing on main itself; an env toggle round one pass in one binary named the pass.
- A debug print every 0.2 s of the pilot's mode, the route's link and its ends, plus the
  command's keys, onground and waterlevel, finds a fall's cause in one rerun; strip it before
  the commit.
