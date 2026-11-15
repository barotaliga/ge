"""The map editors' text (`.map`) read and written, as the engine's C++ reads and writes it
(plugins/brush/compile/map_text.hpp): entities in braces, `"key" "value"` lines, brushes in
braces, one side a line in the classic form or the 220 form. The writer always uses the 220 form.

ge:summary The map SDK's .map text: read (classic and 220 forms) and write (220)
"""

from __future__ import annotations

import re
from typing import List, Tuple

from .geom import Brush, Side, Vec, add, axial_axes, cross, dot, length, mul, norm, sub

_TOKEN = re.compile(r'"([^"]*)"|(//[^\n]*)|([(){}\[\]])|([^\s(){}\[\]"]+)')


class MapTextError(ValueError):
    pass


def _tokens(text: str):
    line = 1
    pos = 0
    for m in _TOKEN.finditer(text):
        line += text.count("\n", pos, m.start())
        pos = m.start()
        if m.group(2) is not None:
            continue
        if m.group(1) is not None:
            yield ("q", m.group(1), line)
        else:
            yield ("w", m.group(3) or m.group(4), line)


def _plane(p0: Vec, p1: Vec, p2: Vec) -> Tuple[Vec, float]:
    n = cross(sub(p0, p1), sub(p2, p1))
    if length(n) < 1e-9:
        raise ValueError("the side's three points are in a line")
    n = norm(n)
    return n, dot(n, p1)


def read(text: str) -> List[Tuple[List[Tuple[str, str]], List[Brush]]]:
    """Every entity as (keys, brushes), the world first. Raises MapTextError naming the line."""
    toks = list(_tokens(text))
    i = 0

    def need(word: str):
        nonlocal i
        if i >= len(toks) or toks[i][1] != word or toks[i][0] != "w":
            found = toks[i][1] if i < len(toks) else "the end"
            line = toks[i][2] if i < len(toks) else (toks[-1][2] if toks else 1)
            raise MapTextError(f"line {line}: expected `{word}`, found `{found}`")
        i += 1

    def number() -> float:
        nonlocal i
        kind, word, line = toks[i]
        try:
            v = float(word)
        except ValueError:
            raise MapTextError(f"line {line}: expected a number, found `{word}`") from None
        i += 1
        return v

    def point() -> Vec:
        need("(")
        p = (number(), number(), number())
        need(")")
        return p

    out = []
    while i < len(toks):
        need("{")
        keys: List[Tuple[str, str]] = []
        brushes: List[Brush] = []
        while i < len(toks) and not (toks[i][0] == "w" and toks[i][1] == "}"):
            if toks[i][0] == "q":
                keys.append((toks[i][1], toks[i + 1][1]))
                i += 2
                continue
            need("{")
            sides: List[Side] = []
            while not (toks[i][0] == "w" and toks[i][1] == "}"):
                line = toks[i][2]
                p0, p1, p2 = point(), point(), point()
                try:
                    n, d = _plane(p0, p1, p2)
                except ValueError as e:
                    raise MapTextError(f"line {line}: {e}") from None
                tex = toks[i][1]
                i += 1
                side = Side(n, d, tex)
                if toks[i][1] == "[":
                    ax = []
                    for _ in range(2):
                        need("[")
                        ax.append((number(), number(), number(), number()))
                        need("]")
                    number()  # the turn, carried by the axes
                    su, sv = number() or 1.0, number() or 1.0
                    side.u, side.v = ax[0][:3], ax[1][:3]
                    side.ou, side.ov, side.su, side.sv = ax[0][3], ax[1][3], su, sv
                else:
                    xo, yo, turn, sx, sy = (number() for _ in range(5))
                    side.u, side.v = _turned(n, turn)
                    side.ou, side.ov, side.su, side.sv = xo, yo, sx or 1.0, sy or 1.0
                if toks[i][0] == "w" and toks[i][1] not in ("(", "}"):
                    number()
                    side.flags, side.value = int(number()), int(number())
                sides.append(side)
            need("}")
            brushes.append(Brush(sides))
        need("}")
        out.append((keys, brushes))
    if not out:
        raise MapTextError("line 1: no entity; write the world first")
    return out


def _turned(n: Vec, turn: float):
    import math

    s, t = axial_axes(n)
    if turn:
        r = math.radians(turn)
        c, k = math.cos(r), math.sin(r)
        s, t = (sub(mul(s, c), mul(t, k)), add(mul(s, k), mul(t, c)))
    return s, t


def _num(v: float) -> str:
    if v == 0:
        return "0"
    r = round(v, 6)
    if r == int(r):
        return str(int(r))
    return repr(r)


def _pt(p: Vec) -> str:
    return "( " + " ".join(_num(c) for c in p) + " )"


def write(entities) -> str:
    """`entities` as (keys, brushes) pairs, the world first, as .map text in the 220 form."""
    out = []
    for ei, (keys, brushes) in enumerate(entities):
        out.append(f"// entity {ei}\n{{")
        seen_version = False
        for k, v in keys:
            if ei == 0 and k == "mapversion":
                v, seen_version = "220", True
            out.append(f'"{k}" "{v}"')
        if ei == 0 and not seen_version:
            out.append('"mapversion" "220"')
        for b in brushes:
            out.append("{")
            for s in b.sides:
                n = s.n
                least = min(range(3), key=lambda k: abs(n[k]))
                a = [0.0, 0.0, 0.0]
                a[least] = 1.0
                u = mul(norm(cross(n, tuple(a))), 64)
                v = cross(n, u)
                p1 = mul(n, s.d)
                ua, va = s.axes()
                line = (f"{_pt(add(p1, u))} {_pt(p1)} {_pt(add(p1, v))} {s.texture} "
                        f"[ {_num(ua[0])} {_num(ua[1])} {_num(ua[2])} {_num(s.ou)} ] "
                        f"[ {_num(va[0])} {_num(va[1])} {_num(va[2])} {_num(s.ov)} ] "
                        f"0 {_num(s.su)} {_num(s.sv)}")
                if s.flags or s.value:
                    line += f" 0 {s.flags} {s.value}"
                out.append(line)
            out.append("}")
        out.append("}")
    return "\n".join(out) + "\n"
