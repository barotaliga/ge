"""A map as a script builds it: brushes, rooms, openings, stairs, liquids, entities, the intent's
areas and tricks; loaded from a .map, cut, merged, moved, mirrored and retextured; saved as
`.map` and `.intent` and built by the engine (`ge map export`).

ge:summary The map SDK's Map: rooms, brushes, entities, intent; load, cut, merge, mix, build
"""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass, field
from typing import Callable, Dict, Iterable, List, Optional, Sequence, Tuple, Union

from . import mapfile
from .geom import (Brush, Side, Vec, add, mirror_side, mul, subtract, turn_side)

Retexture = Union[Dict[str, str], Callable[[str], str]]


@dataclass
class Entity:
    classname: str
    keys: Dict[str, str] = field(default_factory=dict)
    brushes: List[Brush] = field(default_factory=list)

    def origin(self) -> Optional[Vec]:
        o = self.keys.get("origin")
        if not o:
            return None
        x, y, z = (float(v) for v in o.split())
        return (x, y, z)

    def set_origin(self, p: Vec) -> None:
        self.keys["origin"] = " ".join(mapfile._num(c) for c in p)


@dataclass
class Area:
    name: str
    lo: Vec
    hi: Vec
    intent: str
    note: str = ""
    modes: str = ""


@dataclass
class Trick:
    name: str
    frm: Vec
    to: Vec
    kind: str
    level: int = 1
    note: str = ""


def _fmt(v: float) -> str:
    return mapfile._num(v)


def _check_name(what: str, name: str) -> None:
    import re

    if not re.fullmatch(r"[\w.-]+", name):
        word = re.sub(r"[^\w.-]+", "-", name.strip())
        raise ValueError(f"{what} {name!r}: a name is one word (letters, digits, - _ .); "
                         f"write {word!r}")


def _box(lo: Vec, hi: Vec) -> Tuple[Vec, Vec]:
    return tuple(map(min, lo, hi)), tuple(map(max, lo, hi))


class Map:
    """A map being made. The world's brushes in `brushes`, every other entity in `entities`."""

    def __init__(self, message: str = "", textures: Optional[Dict[str, str]] = None):
        self.world: Dict[str, str] = {"classname": "worldspawn"}
        if message:
            self.world["message"] = message
        self.brushes: List[Brush] = []
        self.entities: List[Entity] = []
        self.areas: List[Area] = []
        self.tricks: List[Trick] = []
        self.mode_list: List[str] = []
        # The textures a room's floor, walls and ceiling take unless named.
        self.tex = {"floor": "floor", "wall": "wall", "ceil": "ceil"}
        self.tex.update(textures or {})

    # --- making -------------------------------------------------------------------------------

    def box(self, lo: Vec, hi: Vec, texture: Optional[str] = None) -> Brush:
        """A solid box brush in the world."""
        b = Brush.box(lo, hi, texture or self.tex["wall"])
        self.brushes.append(b)
        return b

    def room(self, lo: Vec, hi: Vec, wall: float = 16, floor: Optional[str] = None,
             walls: Optional[str] = None, ceil: Optional[str] = None) -> "Map":
        """A hollow room: the air from `lo` to `hi`, closed by six slabs `wall` thick. Rooms that
        touch share their slabs' space; open a way between them with `carve` or `door`."""
        lo, hi = _box(lo, hi)
        w = wall
        fl, wl, cl = floor or self.tex["floor"], walls or self.tex["wall"], ceil or self.tex["ceil"]
        self.box((lo[0] - w, lo[1] - w, lo[2] - w), (hi[0] + w, hi[1] + w, lo[2]), fl)
        self.box((lo[0] - w, lo[1] - w, hi[2]), (hi[0] + w, hi[1] + w, hi[2] + w), cl)
        self.box((lo[0] - w, lo[1] - w, lo[2]), (lo[0], hi[1] + w, hi[2]), wl)
        self.box((hi[0], lo[1] - w, lo[2]), (hi[0] + w, hi[1] + w, hi[2]), wl)
        self.box((lo[0], lo[1] - w, lo[2]), (hi[0], lo[1], hi[2]), wl)
        self.box((lo[0], hi[1], lo[2]), (hi[0], hi[1] + w, hi[2]), wl)
        return self

    def carve(self, lo: Vec, hi: Vec) -> "Map":
        """Air from `lo` to `hi`: every world brush there cut away (a door, a window, a pit)."""
        cut = Brush.box(*_box(lo, hi), "wall")
        out: List[Brush] = []
        for b in self.brushes:
            for piece in subtract(b, cut):
                # A new face takes the texture of the side of the brush it was cut from that
                # faces the same way, else the brush's first side's.
                out.append(_retexture_cut(piece, b))
        self.brushes = out
        return self

    door = carve

    def hall(self, lo: Vec, hi: Vec, along: str, wall: float = 16, **tex) -> "Map":
        """A closed way from `lo` to `hi` (a room of its own) running `along` x or y, both ends
        open through its slabs and the walls of the rooms it meets: put its ends on those
        rooms' inner faces."""
        lo, hi = _box(lo, hi)
        if along not in ("x", "y"):
            raise ValueError(f"hall: along is x or y, not {along!r}")
        self.room(lo, hi, wall, **tex)
        k = 0 if along == "x" else 1
        a, b = list(lo), list(hi)
        a[k] -= 2 * wall
        b[k] += 2 * wall
        return self.carve(tuple(a), tuple(b))

    def pit(self, lo: Vec, hi: Vec, liquid: str = "", wall: float = 16, **tex) -> "Map":
        """A sunk room from `lo` up to the floor at `hi`'s height, open at the top through the
        floor above it, filled with `liquid` (water, slime, lava) to one step below the rim."""
        lo, hi = _box(lo, hi)
        self.room(lo, hi, wall, **tex)
        self.carve(lo, (hi[0], hi[1], hi[2] + wall))
        if liquid:
            self.liquid(lo, (hi[0], hi[1], hi[2] - 16), liquid)
        return self

    def stairs(self, lo: Vec, hi: Vec, up: str = "+x", steps: int = 0,
               texture: Optional[str] = None) -> "Map":
        """Steps filling the box from `lo` to `hi`, rising toward `up` (+x, -x, +y, -y), each
        16 high unless `steps` says how many."""
        lo, hi = _box(lo, hi)
        rise = hi[2] - lo[2]
        n = steps or max(1, round(rise / 16))
        axis = 0 if up[1] == "x" else 1
        sign = 1 if up[0] == "+" else -1
        run = (hi[axis] - lo[axis]) / n
        for k in range(n):
            a, b = list(lo), list(hi)
            if sign > 0:
                a[axis] = lo[axis] + k * run
            else:
                b[axis] = hi[axis] - k * run
            b[2] = lo[2] + rise * (k + 1) / n
            self.box(tuple(a), tuple(b), texture or self.tex["floor"])
        return self

    def liquid(self, lo: Vec, hi: Vec, kind: str = "water", texture: Optional[str] = None) -> "Map":
        """A body of water, slime or lava filling the box (carve its pit first)."""
        name = texture or "*" + kind + "1"
        if not name.startswith("*"):
            name = "*" + name
        self.brushes.append(Brush.box(*_box(lo, hi), name))
        return self

    def entity(self, classname: str, at: Optional[Vec] = None, **keys) -> Entity:
        e = Entity(classname, {"classname": classname, **{k: str(v) for k, v in keys.items()}})
        if at is not None:
            e.set_origin(at)
        self.entities.append(e)
        return e

    def light(self, at: Vec, value: int = 300, **keys) -> Entity:
        return self.entity("light", at, light=value, **keys)

    def spawn(self, at: Vec, angle: float = 0, classname: str = "info_player_deathmatch") -> Entity:
        return self.entity(classname, at, angle=_fmt(angle))

    def item(self, classname: str, at: Vec, **keys) -> Entity:
        return self.entity(classname, at, **keys)

    def brush_entity(self, classname: str, brushes: Sequence[Brush], **keys) -> Entity:
        """A door, a plat, a trigger: an entity of its own brushes."""
        e = self.entity(classname, None, **keys)
        e.brushes = list(brushes)
        return e

    # --- the intent ---------------------------------------------------------------------------

    def modes(self, *names: str) -> "Map":
        self.mode_list = list(names)
        return self

    def area(self, name: str, lo: Vec, hi: Vec, intent: str, note: str = "",
             modes: str = "") -> "Map":
        """A place and what it is for: camp, hold, fight, route, power, perch or spawn. The
        name is one word (letters, digits, - _ .)."""
        _check_name("area", name)
        lo, hi = _box(lo, hi)
        self.areas.append(Area(name, lo, hi, intent, note, modes))
        return self

    def trick(self, name: str, frm: Vec, to: Vec, kind: str = "rj", level: int = 1,
              note: str = "") -> "Map":
        """A way the map means a skilled player to take: rj, gj, sj, hj, hop, ... The name is
        one word (letters, digits, - _ .)."""
        _check_name("trick", name)
        self.tricks.append(Trick(name, frm, to, kind, level, note))
        return self

    # --- changing a whole map -----------------------------------------------------------------

    def _all_brushes(self) -> Iterable[Tuple[List[Brush], int]]:
        yield self.brushes, -1
        for i, e in enumerate(self.entities):
            yield e.brushes, i

    def move(self, t: Vec) -> "Map":
        """Everything moved by `t`: brushes, entities, areas and tricks."""
        self.brushes = [b.moved(t) for b in self.brushes]
        for e in self.entities:
            e.brushes = [b.moved(t) for b in e.brushes]
            o = e.origin()
            if o is not None:
                e.set_origin(add(o, t))
        self.areas = [Area(a.name, add(a.lo, t), add(a.hi, t), a.intent, a.note, a.modes)
                      for a in self.areas]
        self.tricks = [Trick(k.name, add(k.frm, t), add(k.to, t), k.kind, k.level, k.note)
                       for k in self.tricks]
        return self

    def mirror(self, axis: str = "x", at: float = 0) -> "Map":
        """Everything mirrored in the plane x = at (or y, z)."""
        k = "xyz".index(axis)

        def m(p: Vec) -> Vec:
            q = list(p)
            q[k] = 2 * at - q[k]
            return tuple(q)

        def mb(b: Brush) -> Brush:
            return Brush([mirror_side(s, k, at) for s in b.sides], b.contents)

        self.brushes = [mb(b) for b in self.brushes]
        for e in self.entities:
            e.brushes = [mb(b) for b in e.brushes]
            o = e.origin()
            if o is not None:
                e.set_origin(m(o))
            if "angle" in e.keys and k < 2 and float(e.keys["angle"]) >= 0:
                a = float(e.keys["angle"])
                e.keys["angle"] = _fmt((180 - a) % 360 if k == 0 else (360 - a) % 360)
        self.areas = [Area(a.name, *_box(m(a.lo), m(a.hi)), a.intent, a.note, a.modes)
                      for a in self.areas]
        self.tricks = [Trick(t.name, m(t.frm), m(t.to), t.kind, t.level, t.note)
                       for t in self.tricks]
        return self

    def turn(self, quarters: int, about: Vec = (0, 0, 0)) -> "Map":
        """Everything turned `quarters` quarter turns anticlockwise (seen from above) about the
        vertical through `about`."""
        q = quarters % 4

        def r(p: Vec) -> Vec:
            x, y, z = p[0] - about[0], p[1] - about[1], p[2]
            for _ in range(q):
                x, y = -y, x
            return (x + about[0], y + about[1], z)

        def rb(b: Brush) -> Brush:
            return Brush([turn_side(s, q, about) for s in b.sides], b.contents)

        self.brushes = [rb(b) for b in self.brushes]
        for e in self.entities:
            e.brushes = [rb(b) for b in e.brushes]
            o = e.origin()
            if o is not None:
                e.set_origin(r(o))
            if "angle" in e.keys and float(e.keys["angle"]) >= 0:
                e.keys["angle"] = _fmt((float(e.keys["angle"]) + 90 * q) % 360)
        self.areas = [Area(a.name, *_box(r(a.lo), r(a.hi)), a.intent, a.note, a.modes)
                      for a in self.areas]
        self.tricks = [Trick(t.name, r(t.frm), r(t.to), t.kind, t.level, t.note)
                       for t in self.tricks]
        return self

    def cut(self, lo: Vec, hi: Vec, seal: Optional[str] = "wall", wall: float = 16) -> "Map":
        """Only the part inside the box kept: brushes clipped to it, entities outside it and
        their brushes dropped, areas and tricks outside it dropped. With `seal`, a shell of
        that texture `wall` thick closes the box, so the piece builds on its own."""
        lo, hi = _box(lo, hi)
        box = Brush.box(lo, hi, seal or "wall")

        def inside(p: Vec) -> bool:
            return all(lo[k] <= p[k] <= hi[k] for k in range(3))

        def clip(bs: List[Brush]) -> List[Brush]:
            out = []
            for b in bs:
                piece: Optional[Brush] = b
                for s in box.sides:
                    if piece is None:
                        break
                    piece = piece.clipped(s)
                if piece is not None:
                    out.append(piece)
            return out

        self.brushes = clip(self.brushes)
        kept = []
        for e in self.entities:
            o = e.origin()
            if e.brushes:
                e.brushes = clip(e.brushes)
                if e.brushes:
                    kept.append(e)
            elif o is None or inside(o):
                kept.append(e)
        self.entities = kept
        self.areas = [a for a in self.areas if inside(a.lo) or inside(a.hi)]
        self.tricks = [t for t in self.tricks if inside(t.frm) and inside(t.to)]
        if seal:
            shell = Map()
            shell.room(lo, hi, wall, seal, seal, seal)
            self.brushes.extend(shell.brushes)
        return self

    def merge(self, other: "Map", offset: Vec = (0, 0, 0)) -> "Map":
        """Another map's brushes, entities, areas and tricks added, moved by `offset` (the
        other's world keys are dropped; a name already used gets -2, -3, ... added)."""
        o = other.copy().move(offset)
        self.brushes.extend(o.brushes)
        self.entities.extend(o.entities)
        names = {a.name for a in self.areas} | {t.name for t in self.tricks}

        def fresh(name: str) -> str:
            k, n = 2, name
            while n in names:
                n, k = f"{name}-{k}", k + 1
            names.add(n)
            return n

        for a in o.areas:
            a.name = fresh(a.name)
            self.areas.append(a)
        for t in o.tricks:
            t.name = fresh(t.name)
            self.tricks.append(t)
        return self

    def retexture(self, how: Retexture) -> "Map":
        """Every side's texture renamed: a dict of old to new names (others kept) or a function
        (mixing one game's map into another's textures)."""
        f = how.get if isinstance(how, dict) else how
        for bs, _ in self._all_brushes():
            for b in bs:
                for s in b.sides:
                    new = f(s.texture) if callable(how) else how.get(s.texture, s.texture)
                    s.texture = new
        return self

    def copy(self) -> "Map":
        import copy

        return copy.deepcopy(self)

    # --- reading and writing ------------------------------------------------------------------

    @staticmethod
    def load(path: str) -> "Map":
        """A .map file (one `ge map import` wrote, or any editor's), and its `.intent` beside it
        when there is one."""
        with open(path, encoding="utf-8", errors="replace") as f:
            ents = mapfile.read(f.read())
        m = Map()
        keys, brushes = ents[0]
        m.world = dict(keys)
        m.brushes = brushes
        for keys, brushes in ents[1:]:
            d = dict(keys)
            m.entities.append(Entity(d.get("classname", ""), d, brushes))
        intent = os.path.splitext(path)[0] + ".intent"
        if os.path.exists(intent):
            m.read_intent(intent)
        return m

    def read_intent(self, path: str) -> "Map":
        import shlex

        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                w = shlex.split(line)
                kv = {k: v for k, _, v in (x.partition("=") for x in w) if v}
                if w[0] == "modes":
                    self.mode_list = w[1].split(",")
                elif w[0] == "area" and "box" in w:
                    z0, z1 = (float(v) for v in kv["z"].split(".."))
                    b = w.index("box")
                    x0, y0 = (float(v) for v in w[b + 1].split(","))
                    x1, y1 = (float(v) for v in w[b + 2].split(","))
                    self.areas.append(Area(w[1], (x0, y0, z0), (x1, y1, z1), kv.get("intent", ""),
                                           kv.get("note", ""), kv.get("modes", "")))
                elif w[0] == "trick":
                    f_ = tuple(float(v) for v in w[w.index("from") + 1].split(","))
                    t_ = tuple(float(v) for v in w[w.index("to") + 1].split(","))
                    self.tricks.append(Trick(w[1], f_, t_, kv.get("kind", "rj"),
                                             int(kv.get("level", "1")), kv.get("note", "")))
        return self

    def map_text(self) -> str:
        ents = [(list(self.world.items()), self.brushes)]
        for e in self.entities:
            keys = dict(e.keys)
            keys.setdefault("classname", e.classname)
            ents.append((list(keys.items()), e.brushes))
        return mapfile.write(ents)

    def intent_text(self) -> str:
        out = []
        if self.mode_list:
            out.append("modes " + ",".join(self.mode_list))
        for a in self.areas:
            line = (f"area {a.name} z={_fmt(a.lo[2])}..{_fmt(a.hi[2])} box {_fmt(a.lo[0])},"
                    f"{_fmt(a.lo[1])} {_fmt(a.hi[0])},{_fmt(a.hi[1])} intent={a.intent}")
            if a.modes:
                line += f" modes={a.modes}"
            if a.note:
                line += f' note="{a.note}"'
            out.append(line)
        for t in self.tricks:
            p = lambda v: ",".join(_fmt(c) for c in v)  # noqa: E731
            line = f"trick {t.name} from {p(t.frm)} to {p(t.to)} kind={t.kind} level={t.level}"
            if t.note:
                line += f' note="{t.note}"'
            out.append(line)
        return "\n".join(out) + "\n"

    def save(self, path: str) -> str:
        """`<path>` (.map) and, when the map has an intent, `<path minus .map>.intent`."""
        with open(path, "w", encoding="utf-8") as f:
            f.write(self.map_text())
        if self.areas or self.tricks or self.mode_list:
            with open(os.path.splitext(path)[0] + ".intent", "w", encoding="utf-8") as f:
                f.write(self.intent_text())
        return path

    def build(self, path: str, out: Optional[str] = None, version: int = 29,
              textures: Optional[str] = None, light: bool = True, ge: Optional[str] = None) -> str:
        """Saved to `path`, then built into a map by the engine (`ge map export`). Returns the
        .bsp's path; raises RuntimeError with the engine's answer when the build fails (a leak:
        the way out is in the answer)."""
        self.save(path)
        exe = _find_ge(ge)
        out = out or os.path.splitext(path)[0] + ".bsp"
        cmd = [exe, "map", "export", path, f"--out={out}", f"--version={version}"]
        if textures:
            cmd.append(f"--textures={textures}")
        if not light:
            cmd.append("--no-light")
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(r.stdout + r.stderr)
        return out

    @staticmethod
    def import_bsp(name: str, path: str, root: Optional[str] = None, game: Optional[str] = None,
                   ge: Optional[str] = None) -> "Map":
        """A built map of any family brought in through the engine (`ge map import`) and loaded."""
        cmd = [_find_ge(ge), "map", "import", name, f"--out={path}"]
        if root:
            cmd.append(f"--root={root}")
        if game:
            cmd.append(f"--game={game}")
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(r.stdout + r.stderr)
        return Map.load(path)


def _find_ge(ge: Optional[str]) -> str:
    exe = ge or os.environ.get("GE") or shutil.which("ge") or _repo_ge()
    if not exe:
        raise RuntimeError("no ge to run: build it (make build), or pass ge= or set GE to its path")
    return exe


def _repo_ge() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", "..", ".."))
    for preset in ("release", "debug"):
        p = os.path.join(root, "build", preset, "ge")
        if os.path.exists(p):
            return p
    return ""


def _retexture_cut(piece: Brush, source: Brush) -> Brush:
    """A cut piece's new sides (the ones `source` did not have) textured like `source`'s side
    facing nearest the same way, so a door's jambs look like the wall they pass through."""
    from dataclasses import replace

    from .geom import dot

    known = {(s.n, s.d) for s in source.sides}
    sides = []
    for s in piece.sides:
        if (s.n, s.d) not in known:
            best = max(source.sides, key=lambda q: dot(q.n, s.n))
            s = replace(s, texture=best.texture)
        sides.append(s)
    return Brush(sides, piece.contents)
