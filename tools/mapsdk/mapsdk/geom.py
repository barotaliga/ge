"""The map SDK's geometry: planes, convex brushes and their sides, cut and moved.

A brush is convex: a list of sides, each a plane facing OUT of the brush (n . p = d, the solid
behind it) with a texture and its projection. Everything is in map units, z up. The engine's
C++ compiler (`ge map export`) does the heavy work (the tree, the seal, light); this module only
builds, cuts and moves brushes, so a script runs in a moment with nothing to compile.

ge:summary The map SDK's geometry: planes, convex brushes, sides, subtract, clip, transforms
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field, replace
from typing import Iterable, List, Optional, Sequence, Tuple

Vec = Tuple[float, float, float]
EPS = 0.01


def add(a: Vec, b: Vec) -> Vec:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a: Vec, b: Vec) -> Vec:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a: Vec, s: float) -> Vec:
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a: Vec, b: Vec) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec, b: Vec) -> Vec:
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def length(a: Vec) -> float:
    return math.sqrt(dot(a, a))


def norm(a: Vec) -> Vec:
    n = length(a)
    return (a[0] / n, a[1] / n, a[2] / n)


def axial_axes(n: Vec) -> Tuple[Vec, Vec]:
    """The texture's axes for a face facing `n`, as the classic editors pick them: a floor or a
    ceiling s along +x and t along -y; a wall facing +-x s along +y, facing +-y s along +x, t down."""
    best, axes = -2.0, ((1, 0, 0), (0, -1, 0))
    for normal, s, t in (
        ((0, 0, 1), (1, 0, 0), (0, -1, 0)),
        ((0, 0, -1), (1, 0, 0), (0, -1, 0)),
        ((1, 0, 0), (0, 1, 0), (0, 0, -1)),
        ((-1, 0, 0), (0, 1, 0), (0, 0, -1)),
        ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
        ((0, -1, 0), (1, 0, 0), (0, 0, -1)),
    ):
        d = dot(n, normal)
        if d > best + 1e-9:
            best, axes = d, (s, t)
    return axes


@dataclass
class Side:
    """One side of a brush: its plane (facing out), texture and projection.

    The projection is the 220 form's: s = (u . p) / su + ou, t = (v . p) / sv + ov in texels.
    `u` and `v` None mean the axial ones for the plane. `flags` and `value` are Quake II's
    surface flags and value (0 for Quake)."""

    n: Vec
    d: float
    texture: str = "wall"
    u: Optional[Vec] = None
    v: Optional[Vec] = None
    ou: float = 0.0
    ov: float = 0.0
    su: float = 1.0
    sv: float = 1.0
    flags: int = 0
    value: int = 0

    def axes(self) -> Tuple[Vec, Vec]:
        if self.u is not None and self.v is not None:
            return self.u, self.v
        return axial_axes(self.n)

    def moved(self, t: Vec) -> "Side":
        """The side moved by `t`, its texture moving with it."""
        u, v = self.axes()
        return replace(self, d=self.d + dot(self.n, t), u=u, v=v,
                       ou=self.ou - dot(u, t) / self.su, ov=self.ov - dot(v, t) / self.sv)

    def flipped(self) -> "Side":
        return replace(self, n=mul(self.n, -1), d=-self.d)


def box_sides(lo: Vec, hi: Vec, texture: str) -> List[Side]:
    return [
        Side((-1, 0, 0), -lo[0], texture), Side((1, 0, 0), hi[0], texture),
        Side((0, -1, 0), -lo[1], texture), Side((0, 1, 0), hi[1], texture),
        Side((0, 0, -1), -lo[2], texture), Side((0, 0, 1), hi[2], texture),
    ]


@dataclass
class Brush:
    sides: List[Side]
    contents: str = ""  # "" from the textures; else water, slime, lava, sky, clip

    @staticmethod
    def box(lo: Vec, hi: Vec, texture: str = "wall") -> "Brush":
        lo, hi = tuple(map(min, lo, hi)), tuple(map(max, lo, hi))
        return Brush(box_sides(lo, hi, texture))

    def corners(self) -> List[Vec]:
        """Every corner: each three sides' planes meeting at a point inside the rest."""
        pts: List[Vec] = []
        s = self.sides
        for i in range(len(s)):
            for j in range(i + 1, len(s)):
                for k in range(j + 1, len(s)):
                    p = _meet(s[i], s[j], s[k])
                    if p is None:
                        continue
                    if all(dot(q.n, p) - q.d <= EPS for q in s):
                        if not any(length(sub(p, o)) < EPS for o in pts):
                            pts.append(p)
        return pts

    def bounds(self) -> Tuple[Vec, Vec]:
        c = self.corners()
        if not c:
            return (0, 0, 0), (0, 0, 0)
        return (tuple(min(p[k] for p in c) for k in range(3)),
                tuple(max(p[k] for p in c) for k in range(3)))

    def solid(self) -> bool:
        """Whether the brush holds a volume (corners not all on one plane)."""
        c = self.corners()
        if len(c) < 4:
            return False
        lo, hi = self.bounds()
        return all(hi[k] - lo[k] > EPS for k in range(3))

    def tidy(self) -> Optional["Brush"]:
        """The brush with sides that touch no corner dropped; None when it holds no volume."""
        if not self.solid():
            return None
        c = self.corners()
        keep = [s for s in self.sides
                if sum(1 for p in c if abs(dot(s.n, p) - s.d) <= EPS) >= 3]
        return Brush(keep, self.contents)

    def clipped(self, side: Side) -> Optional["Brush"]:
        """The part of the brush behind `side`'s plane (the plane becomes a side, its texture
        `side.texture`); None when nothing is left."""
        return Brush(self.sides + [side], self.contents).tidy()

    def moved(self, t: Vec) -> "Brush":
        return Brush([s.moved(t) for s in self.sides], self.contents)

    def textured(self, texture: str) -> "Brush":
        return Brush([replace(s, texture=texture) for s in self.sides], self.contents)


def _meet(a: Side, b: Side, c: Side) -> Optional[Vec]:
    den = dot(a.n, cross(b.n, c.n))
    if abs(den) < 1e-9:
        return None
    p = add(add(mul(cross(b.n, c.n), a.d), mul(cross(c.n, a.n), b.d)), mul(cross(a.n, b.n), c.d))
    return mul(p, 1 / den)


def subtract(b: Brush, cut: Brush, texture: Optional[str] = None) -> List[Brush]:
    """`b` with `cut` taken out of it: the pieces left, each new side textured as the cut's side
    it lies on (or `texture`). A brush the cut misses comes back whole."""
    lo1, hi1 = b.bounds()
    lo2, hi2 = cut.bounds()
    if any(hi1[k] <= lo2[k] + EPS or hi2[k] <= lo1[k] + EPS for k in range(3)):
        return [b]
    out: List[Brush] = []
    rest: Optional[Brush] = b
    for s in cut.sides:
        if rest is None:
            break
        face = replace(s, texture=texture or s.texture)
        front = rest.clipped(face.flipped())  # outside the cut's side: kept
        if front is not None:
            out.append(front)
        rest = rest.clipped(face)
    if rest is None:  # the cut and the brush share no volume: nothing to take out
        return [b]
    return out


def mirror_side(s: Side, axis: int, at: float) -> Side:
    """`s` mirrored in the plane coordinate[axis] = at."""
    n = list(s.n)
    n[axis] = -n[axis]
    d = s.d - 2 * s.n[axis] * at
    u, v = s.axes()
    u2, v2 = list(u), list(v)
    u2[axis], v2[axis] = -u2[axis], -v2[axis]
    ou = s.ou + 2 * u[axis] * at / s.su
    ov = s.ov + 2 * v[axis] * at / s.sv
    return replace(s, n=tuple(n), d=d, u=tuple(u2), v=tuple(v2), ou=ou, ov=ov)


def turn_side(s: Side, quarters: int, about: Vec) -> Side:
    """`s` turned `quarters` quarter turns anticlockwise about the vertical through `about`."""
    q = quarters % 4

    def rot(p: Vec) -> Vec:
        x, y, z = p
        for _ in range(q):
            x, y = -y, x
        return (x, y, z)

    side = s.moved(mul(about, -1))
    u, v = side.axes()
    side = replace(side, n=rot(side.n), u=rot(u), v=rot(v))
    return side.moved(about)
