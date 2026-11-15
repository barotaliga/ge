"""The map SDK's tests: brushes cut and moved, the .map text both ways, rooms sealed, the whole-map
changes, the intent's text, and (when a `ge` is built) a script's map built for real.

    python3 -m unittest discover -s tools/mapsdk -p 'test_*.py'      (make mapsdk-check)

ge:summary The map SDK's tests: geometry, .map text, rooms, transforms, intent, a real build
ge:keywords map-sdk, mapsdk-test, map-text, subtract
ge:skill map-design
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "examples"))

from mapsdk import Brush, Map, subtract  # noqa: E402
from mapsdk import mapfile  # noqa: E402
from mapsdk.geom import dot  # noqa: E402


def volume(b: Brush) -> float:
    lo, hi = b.bounds()
    return (hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2])


def solid_at(brushes, p) -> bool:
    return any(all(dot(s.n, p) - s.d < -0.01 for s in b.sides) for b in brushes)


class Geometry(unittest.TestCase):
    def test_box_corners_and_bounds(self):
        b = Brush.box((0, 0, 0), (64, 32, 16))
        self.assertEqual(len(b.corners()), 8)
        self.assertEqual(b.bounds(), ((0, 0, 0), (64, 32, 16)))

    def test_subtract_keeps_the_volume_outside_the_cut(self):
        wall = Brush.box((0, 0, 0), (16, 512, 256))
        door = Brush.box((-8, 192, 0), (24, 320, 128))
        pieces = subtract(wall, door)
        self.assertEqual(len(pieces), 3)
        self.assertAlmostEqual(sum(volume(p) for p in pieces), 16 * 512 * 256 - 16 * 128 * 128)
        self.assertFalse(solid_at(pieces, (8, 256, 64)))
        self.assertTrue(solid_at(pieces, (8, 256, 200)))

    def test_subtract_a_miss_gives_the_brush_back(self):
        b = Brush.box((0, 0, 0), (16, 16, 16))
        self.assertEqual(subtract(b, Brush.box((32, 0, 0), (48, 16, 16))), [b])

    def test_a_moved_side_keeps_its_texture_in_place(self):
        b = Brush.box((0, 0, 0), (64, 64, 64), "floor")
        top = b.moved((16, 8, 0)).sides[5]
        u, v = top.axes()
        # The texel under the moved corner is the one under the old corner.
        p_old, p_new = (0, 64, 64), (16, 72, 64)
        s_old = dot(u, p_old) / b.sides[5].su + b.sides[5].ou
        s_new = dot(u, p_new) / top.su + top.ou
        self.assertAlmostEqual(s_old, s_new)


class MapText(unittest.TestCase):
    def test_round_trip(self):
        m = Map("Round", textures={"floor": "f1", "wall": "w1"})
        m.room((0, 0, 0), (256, 256, 128))
        m.spawn((32, 32, 24), angle=90)
        text = m.map_text()
        ents = mapfile.read(text)
        self.assertEqual(len(ents), 2)
        self.assertEqual(dict(ents[0][0])["mapversion"], "220")
        self.assertEqual(len(ents[0][1]), 6)
        for a, b in zip(m.brushes, ents[0][1]):
            for sa, sb in zip(a.sides, b.sides):
                self.assertEqual(sa.texture, sb.texture)
                for k in range(3):
                    self.assertAlmostEqual(sa.n[k], sb.n[k])
                self.assertAlmostEqual(sa.d, sb.d)
        self.assertEqual(mapfile.write(ents), text)  # read and written again: the same text

    def test_classic_form_is_read(self):
        text = """{
"classname" "worldspawn"
{
( 0 0 64 ) ( 0 64 64 ) ( 64 0 64 ) rock 0 0 0 1 1
( 0 0 0 ) ( 64 0 0 ) ( 0 64 0 ) rock 0 0 0 1 1
( 0 0 0 ) ( 0 0 64 ) ( 64 0 0 ) rock 0 0 0 1 1
( 0 64 0 ) ( 64 64 0 ) ( 0 64 64 ) rock 0 0 0 1 1
( 0 0 0 ) ( 0 64 0 ) ( 0 0 64 ) rock 0 0 0 1 1
( 64 0 0 ) ( 64 0 64 ) ( 64 64 0 ) rock 0 0 0 1 1
}
}
"""
        (keys, brushes), = mapfile.read(text)
        self.assertEqual(brushes[0].bounds(), ((0, 0, 0), (64, 64, 64)))

    def test_a_fault_names_its_line(self):
        with self.assertRaises(mapfile.MapTextError) as e:
            mapfile.read('{\n"classname" "worldspawn"\n{\n( 0 0 x ) ( 0 0 0 ) ( 0 0 0 ) a 0 0 0 1 1\n}\n}\n')
        self.assertIn("line 4", str(e.exception))


class Making(unittest.TestCase):
    def test_a_room_is_closed(self):
        m = Map()
        m.room((0, 0, 0), (256, 256, 128))
        for p in ((-8, 128, 64), (264, 128, 64), (128, -8, 64), (128, 264, 64),
                  (128, 128, -8), (128, 128, 136)):
            self.assertTrue(solid_at(m.brushes, p), p)
        self.assertFalse(solid_at(m.brushes, (128, 128, 64)))

    def test_a_hall_opens_both_rooms_and_stays_closed(self):
        m = Map()
        m.room((0, 0, 0), (512, 512, 192)).room((640, 0, 0), (1152, 512, 192))
        m.hall((512, 192, 0), (640, 320, 128), "x")
        for x in (504, 520, 576, 632, 648):
            self.assertFalse(solid_at(m.brushes, (x, 256, 64)), x)  # the way through
        for p in ((576, 184, 64), (576, 328, 64), (576, 256, 136), (576, 256, -8),
                  (520, 176, 64), (632, 336, 64)):
            self.assertTrue(solid_at(m.brushes, p), p)  # its sides, top and floor
        with self.assertRaises(ValueError):
            m.hall((0, 0, 0), (1, 1, 1), "z")

    def test_stairs_rise_by_steps(self):
        m = Map()
        m.stairs((0, 0, 0), (128, 64, 64), up="+x")
        self.assertEqual(len(m.brushes), 4)
        self.assertEqual([b.bounds()[1][2] for b in m.brushes], [16, 32, 48, 64])

    def test_a_pit_holds_its_liquid_below_the_rim(self):
        m = Map()
        m.room((0, 0, 0), (512, 512, 128))
        m.pit((128, 128, -64), (384, 384, 0), liquid="water")
        water = [b for b in m.brushes if b.sides[0].texture.startswith("*")]
        self.assertEqual(len(water), 1)
        self.assertEqual(water[0].bounds(), ((128, 128, -64), (384, 384, -16)))
        self.assertFalse(solid_at([b for b in m.brushes if b not in water], (256, 256, -8)))
        self.assertTrue(solid_at(m.brushes, (256, 256, -72)))

    def test_names_are_one_word(self):
        with self.assertRaises(ValueError) as e:
            Map().area("west ledge", (0, 0, 0), (1, 1, 1), "camp")
        self.assertIn("west-ledge", str(e.exception))


class WholeMap(unittest.TestCase):
    def wing(self) -> Map:
        w = Map()
        w.room((0, 0, 0), (256, 256, 128))
        w.spawn((32, 64, 24), angle=30)
        w.area("nook", (0, 0, 0), (256, 256, 128), "camp")
        w.trick("up", (16, 16, 24), (200, 16, 100), "rj", 2)
        return w

    def test_mirror_reflects_brushes_entities_and_intent(self):
        m = self.wing().mirror("x", 512)
        self.assertEqual(m.brushes[0].bounds(), ((752, -16, -16), (1040, 272, 0)))
        self.assertEqual(m.entities[0].origin(), (992, 64, 24))
        self.assertEqual(m.entities[0].keys["angle"], "150")
        self.assertEqual((m.areas[0].lo, m.areas[0].hi), ((768, 0, 0), (1024, 256, 128)))
        self.assertEqual(m.tricks[0].to, (824, 16, 100))

    def test_turn_a_quarter(self):
        m = self.wing().turn(1, (0, 0, 0))
        self.assertEqual(m.entities[0].origin(), (-64, 32, 24))
        self.assertEqual(m.entities[0].keys["angle"], "120")
        lo, hi = m.brushes[0].bounds()
        self.assertEqual((lo, hi), ((-272, -16, -16), (16, 272, 0)))

    def test_merge_numbers_a_used_name(self):
        m = self.wing()
        m.merge(self.wing(), (512, 0, 0)).merge(self.wing(), (1024, 0, 0))
        self.assertEqual([a.name for a in m.areas], ["nook", "nook-2", "nook-3"])
        self.assertEqual(m.entities[1].origin(), (544, 64, 24))

    def test_cut_keeps_the_inside_and_seals_it(self):
        m = self.wing().merge(self.wing(), (512, 0, 0))
        m.cut((-64, -64, -64), (320, 320, 192))
        self.assertEqual(len(m.entities), 1)
        self.assertEqual([a.name for a in m.areas], ["nook"])
        self.assertTrue(solid_at(m.brushes, (330, 128, 64)))  # the seal
        self.assertFalse(solid_at(m.brushes, (128, 128, 64)))


class Intent(unittest.TestCase):
    def test_saved_and_loaded(self):
        m = Map("Saved")
        m.room((0, 0, 0), (256, 256, 128))
        m.modes("duel", "2on2")
        m.area("nook", (0, 0, 0), (256, 256, 128), "camp", "the armour")
        m.trick("up", (16, 16, 24), (200, 16, 100), "rj", 2, "from the corner")
        with tempfile.TemporaryDirectory() as d:
            path = m.save(os.path.join(d, "saved.map"))
            with open(os.path.join(d, "saved.intent"), encoding="utf-8") as f:
                text = f.read()
            self.assertIn('area nook z=0..128 box 0,0 256,256 intent=camp note="the armour"', text)
            back = Map.load(path)
        self.assertEqual(back.mode_list, ["duel", "2on2"])
        self.assertEqual(back.areas[0].note, "the armour")
        self.assertEqual(back.tricks[0].frm, (16, 16, 24))
        self.assertEqual(len(back.brushes), 6)


def _ge():
    for preset in ("release", "debug"):
        p = os.path.join(HERE, "..", "..", "build", preset, "ge")
        if os.path.exists(p):
            return p
    return shutil.which("ge")


@unittest.skipUnless(_ge() and os.environ.get("MAPSDK_BUILD") != "0",
                     "no ge built (make build), or MAPSDK_BUILD=0")
class Build(unittest.TestCase):
    def test_the_example_builds(self):
        from crossfire import crossfire

        with tempfile.TemporaryDirectory() as d:
            bsp = crossfire().build(os.path.join(d, "crossfire.map"), ge=_ge())
            self.assertTrue(os.path.getsize(bsp) > 10000)
            info = subprocess.run([_ge(), "map", "info", bsp], capture_output=True, text=True)
            self.assertIn("family 29", info.stdout)

    def test_a_leak_is_refused_with_its_place(self):
        m = Map()
        m.room((0, 0, 0), (256, 256, 128))
        m.carve((248, 64, 0), (272, 128, 64))  # a hole to the void
        m.spawn((64, 64, 24))
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(RuntimeError) as e:
                m.build(os.path.join(d, "leak.map"), ge=_ge())
        self.assertIn("leaks", str(e.exception))


if __name__ == "__main__":
    unittest.main()
