"""Crossfire: a small duel map written with the map SDK, to read as an example.

Two wings, one the other's mirror, each a low room with a ledge reached by steps, open by a
hall onto a tall atrium. A pool in the atrium's middle holds a pillar with the mega health on
it: reached by a rocket jump from the pool's rim, or by swimming and the steps on its side.

    python3 tools/mapsdk/examples/crossfire.py build/maps/crossfire.map

writes crossfire.map and crossfire.intent there and builds crossfire.bsp beside them.

ge:summary The map SDK's example: a mirrored duel map with a pool, ledges and an intent
ge:keywords map-sdk, example, duel, mirror, intent
ge:skill map-design
ge:test tools/mapsdk/test_mapsdk.py
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from mapsdk import Map  # noqa: E402


def crossfire() -> Map:
    m = Map("Crossfire", textures={"floor": "floor", "wall": "wall", "ceil": "ceil"})
    m.modes("duel")

    # The atrium, 768 by 768 and 384 tall, its pool and the pillar standing in it.
    m.room((640, 0, 0), (1408, 768, 384))
    m.pit((896, 256, -96), (1152, 512, 0), liquid="water")
    m.box((976, 336, -96), (1072, 432, 64))        # the pillar
    m.stairs((1072, 352, -96), (1136, 416, 48), up="-x")  # its steps out of the water

    # The west wing on a map of its own, then copied in twice: as it is and mirrored.
    w = Map(textures=dict(m.tex))
    w.room((0, 0, 0), (512, 768, 320))
    w.box((0, 512, 0), (512, 768, 128))              # the ledge along the north wall
    w.stairs((320, 384, 0), (512, 512, 128), up="+y")
    w.item("weapon_rocketlauncher", (128, 128, 24))
    w.item("item_armor2", (96, 640, 152))
    w.item("item_shells", (448, 64, 24))
    w.spawn((64, 64, 24), angle=45)
    w.spawn((448, 704, 152), angle=225)
    w.light((256, 256, 280), 350)
    w.light((256, 640, 280), 250)
    w.area("ledge", (0, 512, 128), (512, 768, 320), "camp", "the yellow armour; sees the hall")
    m.merge(w).merge(w.copy().mirror("x", 1024))  # the copy's area is named ledge-2

    # The halls from each wing into the atrium, cut through both rooms' walls.
    m.hall((512, 256, 0), (640, 512, 128), "x")
    m.hall((1408, 256, 0), (1536, 512, 128), "x")

    m.item("item_health", (1024, 384, 88), spawnflags=2)  # the mega on the pillar
    m.item("weapon_grenadelauncher", (1024, 64, 24))
    m.item("weapon_grenadelauncher", (1024, 704, 24))
    m.spawn((1024, 160, 24), angle=90)
    m.light((1024, 384, 340), 400)
    m.light((768, 384, 200), 250)
    m.light((1280, 384, 200), 250)

    m.area("atrium", (640, 0, 0), (1408, 768, 384), "fight", "the pool and the open floor")
    m.area("pillar", (976, 336, 64), (1072, 432, 160), "power", "the mega health")
    m.trick("rim-to-pillar", (848, 384, 24), (1024, 384, 88), "rj", 1,
            "a rocket jump from the pool's rim onto the mega")
    return m


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "crossfire.map"
    m = crossfire()
    if "--save-only" in sys.argv:
        print(m.save(out))
    else:
        print(m.build(out))
