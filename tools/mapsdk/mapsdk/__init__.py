"""THE MAP SDK: maps written as Python scripts by agents, built by the engine.

A script makes a `Map` (rooms, brushes, openings, stairs, liquids, lights, items, starts, and the
intent's areas and tricks), or loads one `ge map import` brought in from any built map, then cuts,
merges, moves, mirrors, turns or retextures it, and saves it as the map editors' `.map` text and
an `.intent`. `Map.build` hands the text to the engine's compiler (`ge map export`), which makes
the tree, seals it, builds the hulls and lights it. Standard library only; nothing to compile.

    from mapsdk import Map
    m = Map("Two Rooms", textures={"floor": "floor01_5", "wall": "wbrick1_5"})
    m.room((0, 0, 0), (512, 512, 192)).room((640, 0, 0), (1152, 512, 192))
    m.carve((496, 192, 0), (656, 320, 128))          # the way through both walls
    m.spawn((64, 64, 24))                           # an entity comes back: set its keys
    m.light((256, 256, 160), 300)
    m.area("west", (0, 0, 0), (512, 512, 192), "fight")
    m.build("two_rooms.map")                        # two_rooms.bsp beside it

Docs: docs/levels.md § The map SDK; skill: map-design.

ge:summary The map SDK: maps written as Python scripts, saved as .map text, built by ge
"""

from .builder import Area, Entity, Map, Trick
from .geom import Brush, Side, subtract

__all__ = ["Map", "Entity", "Area", "Trick", "Brush", "Side", "subtract"]
