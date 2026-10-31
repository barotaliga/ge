# weapon_kit: what every Shooter's Edge held-weapon builder (content/models/view/<name>/build.py)
# is made of, run inside headless Blender by `ge asset build <name>` beside the engine's ge_asset.
# A builder adds parts and materials to a Kit and calls `finish`; the kit joins the parts, unwraps
# them, bakes every material into ONE texture (and a glow map), and exports the .glb with the
# texture in it, which the game keeps whole as the held skin (games/shooters_edge/view/weapons.hpp).
#
# THE FRAME is Blender's, in metres, at the weapon's true size, and it is the VIEW's: +X to the
# right, +Y away from the eye (the muzzle's way), +Z up. The game's mount (weapons.hpp § THE MOUNT)
# keeps it so and scales the weapon to its held length. In first person the player sees the
# weapon's LEFT side (-X) and its top: spend the detail there. The melee weapon stands upright in
# the same frame, its edge toward +Y.
#
# THE PARTS are meshes built with bmesh and given a real bevel on their hard edges (the bevel
# modifier, limited by angle, its normals hardened so flat faces shade flat and the bevels catch
# the light): `box`, `lathe` (a profile turned round an axis: barrels with their bore, rings,
# coils, cones), `prism` (a side profile pushed across X: grips, stocks, guards, blades) and
# `loft` (rounded sections along Y: stocks, handles, fore-ends). Every part names its material.
#
# THE MATERIALS are procedural Cycles node graphs (`surface`): a base colour with large mottling,
# streaks along an axis (brushing, scratches) or a grain (wood), knurling or checkering, the EDGES
# worn to another colour (the bevel shader against the surface's normal, broken up by noise), the
# CAVITIES darkened (ambient occlusion), and optionally GLOWING. `finish` bakes their colour as
# emission into the texture, so what the engine draws is exactly the graph's colour with its
# wear and dirt in it; the engine's own light (engine/viewmodel/light.hpp) does the shading.
#
# THE HANDS AND ARMS (`hand`) are ONE body for every weapon: the same gloves, cuffs, sleeves, sizes
# and materials, coarse on purpose, the left the right's mirror. An arm is a chain of rigid nodes
# jointed where a body is: the SHOULDER (a `torso` node, still unless a clip moves it, as an axe's
# swing does), the ELBOW and the WRIST. Its shoulder is where the game's view has it for this
# weapon (the `se_view` data set: the shoulders from the eye, the weapon's offset and the one scale
# every built weapon is drawn at); its hand holds the part it is told to (a pump's hand rides the
# pump), and every frame of every clip the elbow is found by two-bone IK between the two, bent
# by searching its circle for the place inside the arm's ranges of motion (the shoulder, the elbow
# pointing down and out, the wrist, the forearm's twist: engine/viewmodel/arm.hpp, fed as data)
# with the wrist nearest neutral (`finish` prints the rest pose's angles and the worst). A part may turn about a shoulder (pivot "shoulder_r"):
# a swing led by the arm, the wrist kept. The nodes are named `<name>.hand_<side>...`, which the game leaves out of the
# box it measures the weapon by. The weapon's origin is the grip hand's hold.
#
# `kit.stance` turns the body at rest (degrees, + the left shoulder forward): a long gun is held
# bladed, the support hand far up the fore-end. The shoulders stay where the stance puts them while
# the weapon is held; the arms' angles are measured in the turned body's frame.
#
# `kit.size` scales the weapon's parts as they are added (a builder drawn at one size made to
# another), never the hands; a hand's grip is given at the size the weapon ends up.
#
# MOVING PARTS AND CLIPS: parts added inside `with kit.group("pump", pivot=...)` are one node
# of their own after the bake (its origin at the pivot, a child of `parent`), so a clip can move
# them; everything else is the `body`, whose pivot a builder may set too (the axe swings whole
# about the hand). `kit.clip("fire", keys)` keys each group's offset from rest and its turn over
# time; the clips export as glTF animations, sampled at 60 a second, which the game bakes into
# held frames and plays (`fire` on every shot: games/shooters_edge/view/hand.hpp). Every clip
# starts and ends at rest.
#
# THE TEXTURE is TEX_W x TEX_H (2:1): the islands are unwrapped by angle and shelf-packed into it
# (`_pack`), each texel square, so a grain or a knurl keeps its shape. Eight weapons of this size
# fit the view-model atlas with its gutters; 1024 x 512 would not (1024 + 1 + 1024 > 2048).
#
# A builder never reads a clock or a seed of its own: the same script gives the same model.
#
# ge:summary The held weapons' Blender kit: bevelled parts, procedural materials, the bake, export
# ge:keywords shooters-edge weapon-kit blender bpy builder hard-surface bevel lathe prism loft
# ge:keywords procedural-material edge-wear cavity ambient-occlusion bake texture glow export
# ge:skill held-weapons assets render

import contextlib
import math

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

import ge_asset

# Two of these a row with the atlas's one-texel gutters, four rows: eight weapons fill the view-model
# atlas (render/morph.hpp's kMorphAtlasSize, 2048, packed by pack_morph_atlas).
TEX_W, TEX_H = 1020, 510
BAKE_SAMPLES = 48


# ── Node helpers ──


def _sock(sockets, ident):
    for s in sockets:
        if s.identifier == ident:
            return s
    return sockets[ident]


class _Graph:
    """A material's node tree with small builders that return output sockets."""

    def __init__(self, mat):
        self.nt = mat.node_tree
        self.nt.nodes.clear()
        self.x = 0

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        n.location = (self.x, 0)
        self.x += 40
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, out, inp):
        self.nt.links.new(out, inp)

    def val(self, v):
        n = self.node("ShaderNodeValue")
        n.outputs[0].default_value = v
        return n.outputs[0]

    def rgb(self, c):
        n = self.node("ShaderNodeRGB")
        n.outputs[0].default_value = (c[0], c[1], c[2], 1.0)
        return n.outputs[0]

    def _in(self, sock, v):
        if isinstance(v, (int, float)):
            sock.default_value = v
        elif isinstance(v, tuple):
            sock.default_value = v if len(v) == 4 else (v[0], v[1], v[2], 1.0)
        else:
            self.link(v, sock)

    def math(self, op, a, b=0.0, clamp=False):
        n = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
        self._in(n.inputs[0], a)
        self._in(n.inputs[1], b)
        return n.outputs[0]

    def mix(self, fac, a, b, blend="MIX"):
        n = self.node("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        self._in(_sock(n.inputs, "Factor_Float"), fac)
        self._in(_sock(n.inputs, "A_Color"), a)
        self._in(_sock(n.inputs, "B_Color"), b)
        return _sock(n.outputs, "Result_Color")

    def coords(self, scale=(1, 1, 1), rot=(0, 0, 0)):
        tc = self.node("ShaderNodeTexCoord")
        mp = self.node("ShaderNodeMapping", vector_type="POINT")
        self.link(tc.outputs["Object"], mp.inputs["Vector"])
        mp.inputs["Scale"].default_value = scale
        mp.inputs["Rotation"].default_value = rot
        return mp.outputs["Vector"]

    def noise(self, vec, scale, detail=4.0, rough=0.5, distort=0.0):
        n = self.node("ShaderNodeTexNoise", noise_dimensions="3D")
        self.link(vec, n.inputs["Vector"])
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distort
        return n.outputs["Fac"]

    def ramp(self, fac, lo, hi):
        """`fac` mapped so `lo` is 0 and `hi` is 1, clamped."""
        n = self.node("ShaderNodeMapRange", clamp=True)
        self._in(n.inputs["Value"], fac)
        n.inputs["From Min"].default_value = lo
        n.inputs["From Max"].default_value = hi
        return n.outputs["Result"]


# ── Materials ──


def surface(name, base, *, mottle=0.12, mottle_scale=12.0, streak=0.0, streak_axis="Y",
            streak_scale=300.0, grain=None, pattern=None, wear=None, wear_amount=0.0,
            wear_width=0.0025, chip_scale=60.0, cavity=0.55, glow=False, stripes=None,
            zone=None):
    """A procedural material whose COLOUR the bake keeps (linear RGB tuples):

    base          the colour of the surface
    mottle        the share large blotches vary it by, at `mottle_scale` a metre
    streak        brushing or scratches: thin noise stretched along `streak_axis`, its share
    grain         wood: (dark colour, rings a metre, along which axis) laid over `base`
    pattern       ("diamond", lines a metre, depth): knurling or checkering cut into it
    wear          the colour its worn edges show (bare metal under paint or bluing)
    wear_amount   how far the wear reaches (0 none .. 1 every edge bare)
    cavity        how dark the occluded creases get (0 none .. 1 black)
    glow          lights itself (drawn at its colour whatever the light; the glow map)
    stripes       (colour, bands a metre): diagonal hazard stripes over `base`
    zone          (axis, from, to, colour): the colour blended in along an axis of the weapon's
                  frame, none at `from` and all of it at `to` (a blade's polished edge)
    """
    mat = bpy.data.materials.get(name)
    if mat is not None:
        return mat
    mat = bpy.data.materials.new(name)
    g = _Graph(mat)
    colour = g.rgb(base)
    if grain is not None:
        # Flat-sawn: rings round an axis along the grain whose centre (the pith) lies well
        # outside the part, so the rings cross its faces as long lines, wavy where the noise
        # bends them, with fine dark pores along them.
        dark, rings, axis = grain
        stretch = {"X": (0.08, 1, 1), "Y": (1, 0.08, 1), "Z": (1, 1, 0.08)}[axis]
        pith = {"X": (0, 0.23, 0.17), "Y": (0.23, 0, 0.17), "Z": (0.23, 0.17, 0)}[axis]
        tc = g.node("ShaderNodeTexCoord")
        mp = g.node("ShaderNodeMapping", vector_type="POINT")
        g.link(tc.outputs["Object"], mp.inputs["Vector"])
        mp.inputs["Location"].default_value = pith
        vec = mp.outputs["Vector"]
        warp = g.noise(g.coords(scale=stretch), 9.0, 3.0, 0.55)
        wave = g.node("ShaderNodeTexWave", wave_type="RINGS", rings_direction=axis)
        g.link(vec, wave.inputs["Vector"])
        wave.inputs["Scale"].default_value = rings / 10.0
        wave.inputs["Distortion"].default_value = 2.0
        wave.inputs["Detail"].default_value = 2.0
        wave.inputs["Detail Scale"].default_value = 1.0
        g.link(g.math("MULTIPLY", warp, 4.0), wave.inputs["Phase Offset"])
        bands = g.math("POWER", wave.outputs["Fac"], 1.6)
        colour = g.mix(bands, colour, g.rgb(dark))
        pores = g.noise(g.coords(scale=stretch), 700.0, 2.0, 0.4)
        colour = g.mix(g.math("MULTIPLY", g.ramp(pores, 0.58, 0.72), 0.35), colour,
                       g.rgb(dark))
    if mottle > 0:
        m = g.noise(g.coords(), mottle_scale, 3.0, 0.55)
        light = g.math("ADD", g.math("MULTIPLY", g.math("SUBTRACT", m, 0.5), 2 * mottle), 1.0)
        colour = g.mix(1.0, colour, _gray(g, light), "MULTIPLY")
    if streak > 0:
        s = {"X": (1, 40, 40), "Y": (40, 1, 40), "Z": (40, 40, 1)}[streak_axis]
        n = g.noise(g.coords(scale=s), streak_scale / 40.0, 6.0, 0.7)
        light = g.math("ADD", g.math("MULTIPLY", g.math("SUBTRACT", n, 0.5), 2 * streak), 1.0)
        colour = g.mix(1.0, colour, _gray(g, light), "MULTIPLY")
    if stripes is not None:
        other, per_m = stripes
        wave = g.node("ShaderNodeTexWave", wave_type="BANDS", bands_direction="DIAGONAL")
        g.link(g.coords(), wave.inputs["Vector"])
        wave.inputs["Scale"].default_value = per_m / 10.0
        sq = g.math("GREATER_THAN", wave.outputs["Fac"], 0.5)
        colour = g.mix(sq, colour, g.rgb(other))
    if pattern is not None:
        kind, per_m, depth = pattern
        a = g.node("ShaderNodeTexWave", wave_type="BANDS", bands_direction="DIAGONAL")
        b = g.node("ShaderNodeTexWave", wave_type="BANDS", bands_direction="DIAGONAL")
        g.link(g.coords(), a.inputs["Vector"])
        g.link(g.coords(scale=(-1, 1, 1)), b.inputs["Vector"])
        for w in (a, b):
            w.inputs["Scale"].default_value = per_m / 10.0
            w.wave_profile = "TRI"
        groove = g.math("MULTIPLY", g.ramp(a.outputs["Fac"], 0.0, 0.25),
                        g.ramp(b.outputs["Fac"], 0.0, 0.25))
        dark = g.math("ADD", g.math("MULTIPLY", groove, depth), 1.0 - depth)
        colour = g.mix(1.0, colour, _gray(g, dark), "MULTIPLY")
    if zone is not None:
        axis, z0, z1, zc = zone
        sep = g.node("ShaderNodeSeparateXYZ")
        g.link(g.coords(), sep.inputs[0])
        k = g.ramp(sep.outputs["XYZ".index(axis)], z0, z1)
        colour = g.mix(k, colour, g.rgb(zc))
    if wear is not None and wear_amount > 0:
        bev = g.node("ShaderNodeBevel", samples=8)
        bev.inputs["Radius"].default_value = wear_width
        geo = g.node("ShaderNodeNewGeometry")
        dot = g.node("ShaderNodeVectorMath", operation="DOT_PRODUCT")
        g.link(bev.outputs["Normal"], dot.inputs[0])
        g.link(geo.outputs["Normal"], dot.inputs[1])
        edge = g.ramp(g.math("SUBTRACT", 1.0, dot.outputs["Value"]), 0.02, 0.12)
        chips = g.noise(g.coords(), chip_scale, 6.0, 0.65)
        reach = g.ramp(chips, 0.62 - 0.3 * wear_amount, 0.68 - 0.3 * wear_amount)
        rub = g.math("MULTIPLY", edge, g.math("ADD", reach, 0.25 * wear_amount), clamp=True)
        scuff = g.noise(g.coords(), chip_scale * 2.5, 4.0, 0.6)
        speck = g.math("MULTIPLY", g.ramp(scuff, 0.72 - 0.15 * wear_amount, 0.76), wear_amount)
        colour = g.mix(g.math("MAXIMUM", rub, speck), colour, g.rgb(wear))
    if cavity > 0:
        ao = g.node("ShaderNodeAmbientOcclusion", samples=12, only_local=True)
        ao.inputs["Distance"].default_value = 0.012
        dirt = g.math("ADD", g.math("MULTIPLY", ao.outputs["AO"], cavity), 1.0 - cavity)
        colour = g.mix(1.0, colour, _gray(g, dirt), "MULTIPLY")
    emit = g.node("ShaderNodeEmission")
    g.link(colour, emit.inputs["Color"])
    glow_emit = g.node("ShaderNodeEmission")
    glow_emit.inputs["Color"].default_value = (1, 1, 1, 1) if glow else (0, 0, 0, 1)
    out = g.node("ShaderNodeOutputMaterial")
    g.link(emit.outputs[0], out.inputs["Surface"])
    target = g.node("ShaderNodeTexImage")
    target.name = "bake_target"
    mat["ge_colour"] = emit.name
    mat["ge_glow"] = glow_emit.name
    mat["ge_glows"] = bool(glow)
    return mat


def _gray(g, v):
    n = g.node("ShaderNodeCombineColor")
    for i in range(3):
        g._in(n.inputs[i], v)
    return n.outputs[0]



# ── The hands and arms: one body for every weapon ──

GLOVE = None
SLEEVE = None
CUFF = None


def _hand_materials():
    global GLOVE, SLEEVE, CUFF
    if GLOVE is None:
        GLOVE = surface("glove", (0.045, 0.040, 0.036), mottle=0.22, mottle_scale=45,
                        streak=0.10, streak_axis="Z", wear=(0.16, 0.13, 0.10), wear_amount=0.55,
                        wear_width=0.003, chip_scale=140, cavity=0.75)
        SLEEVE = surface("sleeve", (0.10, 0.11, 0.075), mottle=0.18, mottle_scale=30,
                         pattern=("diamond", 900.0, 0.18), wear=(0.17, 0.18, 0.13),
                         wear_amount=0.35, cavity=0.7)
        CUFF = surface("cuff", (0.028, 0.026, 0.025), mottle=0.15,
                       pattern=("diamond", 1400.0, 0.3), cavity=0.6)
    return GLOVE, SLEEVE, CUFF


# The body's sizes, true metres: the same on every weapon (the arm's lengths and the shoulders come
# from the game, the `se_view` data set).
FINGER_R = (0.0100, 0.0102, 0.0096, 0.0085)  # index, middle, ring, little
FINGER_LEN = (0.085, 0.092, 0.087, 0.070)     # knuckle to tip
FINGER_AT = (0.028, 0.009, -0.010, -0.028)   # along the grip, index first
PALM_HALF = 0.040
PALM_THICK = 0.014
WRIST_R = (0.029, 0.022)
# How obliquely a grip crosses the palm, as the wrist's place along the grip from the knuckles (m;
# further is more oblique): a POWER grip (a handle, a pistol grip) crosses at about 70 deg to the
# forearm, a CRADLE (a fore-end or a pump lying along the palm) anywhere from about 30 to 60. The
# kit tries each and keeps the one whose arm is most natural.
WRIST_ALONG = {"power": (-0.024, -0.034, -0.046),
               "cradle": (-0.060, -0.075, -0.090, -0.105, -0.120, -0.135)}
UPPER_R = (0.050, 0.046, 0.043)  # the upper arm's radius at the shoulder, the middle, the elbow
FORE_R = (0.041, 0.036, 0.030)   # the forearm's at the elbow, the middle, the wrist
ARM_AWAY = 4.0  # the arm's segments are built this far off along X, out of the bake's shadows
DEFAULTS = {"scale": 0.5, "shoulders": {"r": [0.19, -0.24, -0.06], "l": [-0.19, -0.24, -0.06]},
            "upper_arm": 0.30, "forearm": 0.27}


class _Frame:
    """A grip's frame: `a` along it (toward the index's end), `e1` toward the knuckles' side, `e2`
    the way the fingers go first (round the front); a point at angle phi round the grip is
    cos(phi) e1 + sin(phi) e2, so the fingers wrap from phi 15 deg to past 180."""

    def __init__(self, centre, axis, knuckles):
        self.c = Vector(centre)
        self.a = Vector(axis).normalized()
        k = Vector(knuckles)
        self.e1 = (k - self.a * k.dot(self.a)).normalized()
        self.e2 = self.a.cross(self.e1).normalized()

    def at(self, phi_deg, radius, along):
        p = math.radians(phi_deg)
        return self.c + (self.e1 * math.cos(p) + self.e2 * math.sin(p)) * radius + self.a * along


def _sweep(bm, path, sides=8, up=None, cap_start=True, cap_end=True):
    """A tube through `path` [(point, r_across, r_through)], its sections turned by parallel
    transport from `up` (r_across lies along it), closed with a point at each end."""
    pts = [Vector(p[0]) for p in path]
    rows = []
    n_prev = None
    for i, (p, ra, rb) in enumerate(path):
        p = Vector(p)
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        if n_prev is None:
            u = Vector(up) if up is not None else Vector((0, 0, 1))
            if abs(u.dot(t)) > 0.95:
                u = Vector((1, 0, 0))
            n = (u - t * u.dot(t)).normalized()
        else:
            n = (n_prev - t * n_prev.dot(t)).normalized()
        n_prev = n
        b = t.cross(n)
        row = []
        for k in range(sides):
            a = 2 * math.pi * k / sides
            row.append(bm.verts.new(p + n * (ra * math.cos(a)) + b * (rb * math.sin(a))))
        rows.append((row, p, t))
    for r in range(len(rows) - 1):
        for k in range(sides):
            n2 = (k + 1) % sides
            bm.faces.new((rows[r][0][k], rows[r][0][n2], rows[r + 1][0][n2], rows[r + 1][0][k]))
    for end, keep in ((0, cap_start), (-1, cap_end)):
        if not keep:
            continue
        row, p, t = rows[end]
        tip = bm.verts.new(p + t * (path[end][2] * 0.7) * (1 if end == -1 else -1))
        for k in range(sides):
            n2 = (k + 1) % sides
            bm.faces.new((row[k], row[n2], tip) if end == -1 else (row[n2], row[k], tip))


def _basis(y, x_hint, at):
    """The matrix whose +Y runs along `y`, its +X as near `x_hint` as it can be, at `at`."""
    y = Vector(y).normalized()
    z = Vector(x_hint).cross(y)
    if z.length < 1e-6:
        z = Vector((0, 0, 1)).cross(y)
    z.normalize()
    x = y.cross(z)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(at)
    return m


LIMITS = {"shoulder_elevation_max": 25, "shoulder_yaw_min": -45, "shoulder_yaw_max": 135,
          "elbow_flex_max": 145, "elbow_roll_min": -30, "elbow_roll_max": 100,
          "wrist_extension_max": 70, "wrist_flexion_max": 75, "wrist_radial_max": 20,
          "wrist_ulnar_max": 35, "forearm_twist_max": 95}


# The easy band of each joint (deg) and how dear straying from it is: the elbow pointing down and
# a little out, the wrist near straight, the forearm not wrung, the upper arm not raised.
COMFORT = (("elbow_roll", -10.0, 60.0, 1.0), ("wrist_extension", -20.0, 30.0, 1.5),
           ("wrist_radial", -15.0, 10.0, 2.0), ("forearm_twist", -50.0, 50.0, 1.0),
           ("shoulder_elevation", -90.0, 0.0, 1.0), ("shoulder_yaw", -20.0, 90.0, 0.5),
           ("elbow_flex", 10.0, 120.0, 0.5))


def _view(v):
    """A vector in Blender's frame (x right, y away, z up) in the view's (x right, y up, z away),
    the frame engine/viewmodel/arm.hpp measures in."""
    return Vector((v[0], v[2], v[1]))


def _arm_angles(s, e, w, distal, radial, right, up, fwd, side):
    """The arm's angles, degrees, as engine/viewmodel/arm.cpp's arm_angles (the authority the
    labs and tests hold every frame to): every vector in the view's frame."""
    def deg(r):
        return math.degrees(r)

    def clamp(x):
        return max(-1.0, min(1.0, x))

    out = side * right
    u = (e - s).normalized()
    f = (w - e).normalized()
    a = {"shoulder_elevation": deg(math.asin(clamp(u.dot(up))))}
    a["shoulder_yaw"] = deg(math.atan2(u.dot(out), u.dot(fwd))) if a["shoulder_elevation"] > -60 \
        else 0.0
    a["elbow_flex"] = deg(math.acos(clamp(u.dot(f))))
    bent = a["elbow_flex"] >= 10
    a["elbow_roll"] = 0.0
    if bent:
        line = (w - s).normalized()
        perp = (e - s) - line * (e - s).dot(line)
        perp.normalize()
        a["elbow_roll"] = deg(math.atan2(perp.dot(out), -perp.dot(up)))
    rad = (radial - f * radial.dot(f)).normalized()
    back = rad.cross(f) * side
    hand = distal.normalized()
    a["wrist_extension"] = deg(math.atan2(hand.dot(back), hand.dot(f)))
    a["wrist_radial"] = deg(math.atan2(hand.dot(rad), hand.dot(f)))
    a["forearm_twist"] = 0.0
    if bent:
        crease = (-u) - f * (-u).dot(f)
        crease.normalize()
        a["forearm_twist"] = deg(math.atan2(rad.dot(f.cross(crease)), rad.dot(crease)))
    return a


def _excess(a, lim):
    """Degrees outside the ranges, summed (arm.cpp's arm_excess)."""
    def over(v, lo, hi):
        return lo - v if v < lo else (v - hi if v > hi else 0.0)

    return (over(a["shoulder_elevation"], -90, lim["shoulder_elevation_max"]) +
            over(a["shoulder_yaw"], lim["shoulder_yaw_min"], lim["shoulder_yaw_max"]) +
            over(a["elbow_flex"], 0, lim["elbow_flex_max"]) +
            over(a["elbow_roll"], lim["elbow_roll_min"], lim["elbow_roll_max"]) +
            over(a["wrist_extension"], -lim["wrist_flexion_max"], lim["wrist_extension_max"]) +
            over(a["wrist_radial"], -lim["wrist_ulnar_max"], lim["wrist_radial_max"]) +
            over(a["forearm_twist"], -lim["forearm_twist_max"], lim["forearm_twist_max"]))


def _ik(s, w, l1, l2, judge, prev=None, steps=96):
    """The elbow between shoulder `s` and wrist `w` for arms of `l1` and `l2`: every place on
    its circle tried, `judge(elbow)` scoring each (lower is better), the place nearest `prev`
    (an angle on the circle) winning a near tie so a clip never jumps. Returns the elbow, its
    angle and how far the wrist is past the arm's reach (0: it reaches)."""
    d_vec = w - s
    d = d_vec.length
    u = d_vec / d
    dc = min(max(d, abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
    a = (l1 * l1 - l2 * l2 + dc * dc) / (2 * dc)
    h = math.sqrt(max(l1 * l1 - a * a, 0.0))
    ref = Vector((0, 0, -1)) if abs(u.z) < 0.95 else Vector((1, 0, 0))
    p1 = (ref - u * ref.dot(u)).normalized()
    p2 = u.cross(p1)
    best = None
    for k in range(steps):
        t = 2 * math.pi * k / steps
        e = s + u * a + (p1 * math.cos(t) + p2 * math.sin(t)) * h
        cost = judge(e)
        if prev is not None:
            gap = abs((t - prev + math.pi) % (2 * math.pi) - math.pi)
            cost += 2.0 * gap
        if best is None or cost < best[0]:
            best = (cost, e, t)
    return best[1], best[2], max(0.0, d - (l1 + l2)), best[0]


def _hand_points(frame, grip_r, along):
    """The wrist and the knuckles' middle of the right hand on `frame`'s grip (what the hold
    search judges, and where `_hand_mesh` puts them)."""
    wrist = frame.at(-95, grip_r + 0.050, along)
    knuckles = (frame.at(15, grip_r + 0.012, FINGER_AT[0]) +
                frame.at(15, grip_r + 0.012, FINGER_AT[3])) / 2
    return wrist, knuckles


def _hand_mesh(kit, prefix, frame, grip_r, index_curl, along):
    """The glove's parts round `frame`'s grip (the right hand); returns the wrist and the
    knuckles' middle."""
    glove, _, _ = _hand_materials()

    def add(part, path, sides=8, up=None, caps=(True, True)):
        bm = bmesh.new()
        _sweep(bm, path, sides, up, caps[0], caps[1])
        kit._add(prefix + "_" + part, bm, glove, 0.0)

    for i in range(4):
        r = FINGER_R[i]
        ring = grip_r + r * 0.95
        reach = FINGER_LEN[i] / ring * (index_curl if i == 0 else 1.0)
        path = []
        for k in range(7):
            u = k / 6
            bulge = 1.08 if k in (0, 3, 5) else 1.0
            taper = 1.0 - 0.18 * u
            path.append((frame.at(15 + math.degrees(reach) * u, ring, FINGER_AT[i]),
                         r * bulge * taper, r * 0.9 * bulge * taper))
        add("finger%d" % i, path, sides=6, up=frame.a)
    # The grip crosses the palm obliquely, as in any power grip: from the index's knuckle to the
    # heel of the hand on the little finger's side, about 70 deg to the forearm, so the wrist sits
    # toward the little finger's end of the grip, not straight behind it.
    wrist, knuckles = _hand_points(frame, grip_r, along)
    slab = [(frame.at(12, grip_r + 0.0105, 0.0), PALM_HALF, PALM_THICK),
            (frame.at(-35, grip_r + 0.0125, along * 0.25), PALM_HALF, PALM_THICK * 1.15),
            (frame.at(-72, grip_r + 0.0205, along * 0.6), PALM_HALF * 0.86,
             PALM_THICK * 1.4),
            (wrist, WRIST_R[0], WRIST_R[1])]
    add("palm", slab, sides=8, up=frame.a, caps=(True, False))
    add("knuckles", [(frame.at(15, grip_r + 0.0165, FINGER_AT[0] + 0.006), 0.0075, 0.006),
                     (frame.at(14, grip_r + 0.0175, FINGER_AT[3] - 0.004), 0.0070, 0.006)],
        sides=6)
    thumb = []
    for k in range(6):
        u = k / 5
        rad = grip_r + 0.012 + 0.016 * (1 - u) ** 2
        along = FINGER_AT[0] + 0.012 + 0.006 * math.sin(math.pi * u) - 0.022 * (1 - u)
        r = 0.0118 * (1 - 0.22 * u)
        thumb.append((frame.at(-70 - 125 * u, rad, along), r, r * 0.9))
    add("thumb", thumb, sides=6, up=frame.a)
    return wrist, knuckles


def _segment(kit, name, length, radii, away, end_cap, cuff=False):
    """An arm's segment along +Y from its joint (0 .. length), built `away` m off along X for the
    bake: the sleeve, rounded where the next joint turns, a glove's cuff at a wrist."""
    _, sleeve, cuff_mat = _hand_materials()
    r0, r1, r2 = radii
    rings = [(-0.012, away, 0, r0 * 0.92, r0 * 0.85), (0.0, away, 0, r0, r0 * 0.94),
             (length * 0.5, away, 0, r1, r1 * 0.94), (length - 0.01, away, 0, r2, r2 * 0.94)]
    if end_cap:
        rings += [(length + 0.012, away, 0, r2 * 0.9, r2 * 0.85),
                  (length + 0.026, away, 0, r2 * 0.55, r2 * 0.5)]
    kit.loft(name, rings, sleeve, sides=10)
    if cuff:
        kit.loft(name + "_cuff", [(length - 0.050, away, 0, r2 * 1.06, r2 * 1.0),
                                  (length - 0.004, away, 0, r2 * 1.12, r2 * 1.06),
                                  (length + 0.004, away, 0, r2 * 0.95, r2 * 0.9)],
                 cuff_mat, sides=10)


def _ease(keys, t, linear=False):
    """A group's (offset, turn) at `t` from its keys, smoothstepped between them (or straight, for
    a steady motion such as barrels turning)."""
    if t <= keys[0][0]:
        return Vector(keys[0][1]), Vector(keys[0][2])
    for (t0, d0, r0), (t1, d1, r1) in zip(keys, keys[1:]):
        if t <= t1:
            u = (t - t0) / (t1 - t0) if t1 > t0 else 1.0
            e = u if linear else u * u * (3 - 2 * u)
            return (Vector(d0).lerp(Vector(d1), e), Vector(r0).lerp(Vector(r1), e))
    return Vector(keys[-1][1]), Vector(keys[-1][2])


def _fcurves(action):
    """An action's F-curves: its layers' strips' channel bags (Blender 4.4 and later) or its
    own list."""
    if hasattr(action, "layers") and len(action.layers):
        out = []
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    out.extend(bag.fcurves)
        return out
    return list(action.fcurves)


# ── Parts ──


class Kit:
    def __init__(self, data=None, weapon=None):
        """`data` the `se_view` data set (None: the defaults, for a rig), `weapon` this model's
        name in it (its offset puts the shoulders)."""
        self.parts = []
        self.groups = {"body": {"pivot": (0, 0, 0), "parent": None}}
        self.current = "body"
        self.clips = []
        self.arms = []
        self.size = 1.0  # the weapon's parts (never the hands) scaled by this as they are added
        self.stance = 0.0  # deg the body stands turned, + the left shoulder forward (a long gun)
        self.view = data if data is not None else DEFAULTS
        offset = (0, 0, 0)
        if data is not None and weapon is not None:
            offset = data["weapons"][weapon]["offset"]
        scale = self.view["scale"]
        # The shoulders in this model's frame (Blender's: x right, y away, z up), true metres:
        # where the view has them, less where the view holds this weapon's origin.
        self.shoulders = {}
        for side, v in self.view["shoulders"].items():
            self.shoulders[side] = Vector((v[0] - offset[0] / scale, v[2] - offset[2] / scale,
                                           v[1] - offset[1] / scale))

    @contextlib.contextmanager
    def group(self, name, pivot=(0, 0, 0), parent="body"):
        """Parts added inside are the moving part `name`, turned about `pivot`, a child of
        `parent`. The body's own pivot is set with `kit.groups["body"]["pivot"] = ...`."""
        if name not in self.groups:
            self.groups[name] = {"pivot": tuple(pivot), "parent": parent}
        before = self.current
        self.current = name
        try:
            yield
        finally:
            self.current = before

    def clip(self, name, keys, ease="smooth"):
        """A clip: {group: [(t_s, (dx, dy, dz), (rx, ry, rz)), ...]}, each group's offset from
        rest (m) and its turn (rad, Euler XYZ about its pivot) at each time, smoothstepped between
        keys, or `ease="linear"` for a steady motion (barrels spinning shot after shot)."""
        self.clips.append((name, keys, ease))

    # Every part goes through here: its material, its bevel, smooth shading with hardened normals.
    def _add(self, name, bm, mat, bevel, segments=2, angle=35.0):
        if self.size != 1.0 and not self.current.startswith("hand_"):
            bmesh.ops.scale(bm, vec=(self.size,) * 3, verts=bm.verts)
            bevel *= self.size
        me = bpy.data.meshes.new(name)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(me)
        bm.free()
        obj = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(obj)
        me.materials.append(mat)
        for p in me.polygons:
            p.use_smooth = True
        if bevel > 0:
            mod = obj.modifiers.new("bevel", "BEVEL")
            mod.width = bevel
            mod.segments = segments
            mod.limit_method = "ANGLE"
            mod.angle_limit = math.radians(angle)
            mod.harden_normals = True
            mod.profile = 0.6
            mod.miter_outer = "MITER_ARC"
            mod.use_clamp_overlap = True
        else:
            mod = obj.modifiers.new("normals", "WEIGHTED_NORMAL")
            mod.keep_sharp = True
            mod.weight = 50
        obj["ge_group"] = self.current
        self.parts.append(obj)
        return obj

    @staticmethod
    def _place(bm, loc=(0, 0, 0), rot=(0, 0, 0)):
        m = (Matrix.Translation(Vector(loc)) @ Matrix.Rotation(rot[2], 4, "Z") @
             Matrix.Rotation(rot[1], 4, "Y") @ Matrix.Rotation(rot[0], 4, "X"))
        bmesh.ops.transform(bm, matrix=m, verts=bm.verts)

    def box(self, name, size, loc, mat, bevel=0.0015, rot=(0, 0, 0), taper=None):
        """A box of `size` (x, y, z) round `loc`. `taper` (sx, sz): the +Y end's x and z scaled."""
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x *= size[0]
            v.co.y *= size[1]
            v.co.z *= size[2]
            if taper and v.co.y > 0:
                v.co.x *= taper[0]
                v.co.z *= taper[1]
        self._place(bm, loc, rot)
        return self._add(name, bm, mat, bevel)

    def lathe(self, name, profile, loc, mat, axis="Y", sides=32, bevel=0.0008, rot=None,
              closed=True):
        """`profile` [(radius, along)...] turned round `axis` through `loc`: a closed profile
        (the outside forward, the bore back) makes a solid with a hole; radius 0 sits on the axis.
        """
        bm = bmesh.new()
        verts = [bm.verts.new((r, a, 0.0)) for r, a in profile]
        edges = [bm.edges.new((verts[i], verts[i + 1])) for i in range(len(verts) - 1)]
        if closed and len(verts) > 2:
            edges.append(bm.edges.new((verts[-1], verts[0])))
        bmesh.ops.spin(bm, geom=verts + edges, cent=(0, 0, 0), axis=(0, 1, 0),
                       angle=2 * math.pi, steps=sides, use_merge=True)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
        turn = {"Y": (0, 0, 0), "X": (0, 0, -math.pi / 2), "Z": (math.pi / 2, 0, 0)}[axis]
        self._place(bm, loc, rot if rot is not None else turn)
        return self._add(name, bm, mat, bevel, angle=40.0)

    def tube(self, name, r_out, r_in, length, loc, mat, axis="Y", sides=32, bevel=0.0008,
             rot=None):
        """A hollow cylinder of `length` centred on `loc` along `axis`."""
        h = length / 2
        prof = [(r_in, -h), (r_out, -h), (r_out, h), (r_in, h)] if r_in > 0 else \
            [(0, -h), (r_out, -h), (r_out, h), (0, h)]
        return self.lathe(name, prof, loc, mat, axis, sides, bevel, rot)

    def cyl(self, name, r, length, loc, mat, axis="Y", sides=24, bevel=0.0008, rot=None):
        return self.tube(name, r, 0, length, loc, mat, axis, sides, bevel, rot)

    def prism(self, name, profile, width, mat, x=0.0, bevel=0.0015, loc=(0, 0, 0), width_at=None):
        """A side profile [(y, z)...] (counter-clockwise seen from +X) pushed `width` across X,
        centred on `x`. `width_at(y, z)`, when given, is the width at each profile point instead
        (a blade thinning to its edge)."""
        bm = bmesh.new()
        verts = [bm.verts.new((x - width / 2, p[0], p[1])) for p in profile]
        face = bm.faces.new(verts)
        ext = bmesh.ops.extrude_face_region(bm, geom=[face])
        moved = [e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)]
        bmesh.ops.translate(bm, vec=(width, 0, 0), verts=moved)
        if width_at is not None:
            for v in bm.verts:
                half = width_at(v.co.y, v.co.z) / 2
                v.co.x = x + (half if v.co.x > x else -half)
        bmesh.ops.triangulate(bm, faces=bm.faces, quad_method="BEAUTY", ngon_method="BEAUTY")
        bmesh.ops.join_triangles(bm, faces=bm.faces, angle_face_threshold=0.01,
                                 angle_shape_threshold=3.0)
        self._place(bm, loc)
        return self._add(name, bm, mat, bevel)

    def loft(self, name, rings, mat, sides=24, power=2.0, caps=True, bevel=0.0, rot=None):
        """Sections along Y, each (y, cx, cz, rx, rz): a superellipse of exponent `power` (2 an
        ellipse, higher squarer) across X and Z centred on (cx, cz). A ring may add its own power
        as a sixth value."""
        bm = bmesh.new()
        rows = []
        for ring in rings:
            y, cx, cz, rx, rz = ring[:5]
            pw = ring[5] if len(ring) > 5 else power
            row = []
            for k in range(sides):
                a = 2 * math.pi * k / sides
                c, s = math.cos(a), math.sin(a)
                ex = 2.0 / pw
                px = cx + rx * math.copysign(abs(c) ** ex, c)
                pz = cz + rz * math.copysign(abs(s) ** ex, s)
                row.append(bm.verts.new((px, y, pz)))
            rows.append(row)
        for r in range(len(rows) - 1):
            for k in range(sides):
                n = (k + 1) % sides
                bm.faces.new((rows[r][k], rows[r][n], rows[r + 1][n], rows[r + 1][k]))
        if caps:
            bm.faces.new(rows[0])
            bm.faces.new(list(reversed(rows[-1])))
        if rot is not None:
            self._place(bm, (0, 0, 0), rot)
        return self._add(name, bm, mat, bevel)

    def cable(self, name, points, r, mat, sides=10):
        """A round cable (a hose, a wire bundle, a prong) swept through `points`, closed at its
        ends."""
        bm = bmesh.new()
        _sweep(bm, [(Vector(p), r, r) for p in points], sides)
        return self._add(name, bm, mat, 0.0)

    def screw(self, name, loc, mat, r=0.0025, axis="X", depth=0.0015):
        """A slotted screw head standing out of a face along `axis` (sign in front: "-X")."""
        sign = -1 if axis.startswith("-") else 1
        ax = axis[-1]
        prof = [(0, 0), (r, 0), (r, depth * 0.5), (r * 0.8, depth), (0, depth)]
        prof = [(p[0], p[1] * sign) for p in prof]
        turn = {"Y": (0, 0, 0), "X": (0, 0, -math.pi / 2), "Z": (math.pi / 2, 0, 0)}[ax]
        return self.lathe(name, prof, loc, mat, ax, 12, 0.0, turn, closed=True)

    def hand(self, side, centre, axis, knuckles, grip_r, index_curl=1.0, holds="body",
             pole=None, turn_range=60, grip="power"):
        """The body's `side` ("r" or "l") hand holding a grip of radius `grip_r` whose axis runs
        through `centre` along `axis` (toward the index's end), the knuckles toward `knuckles` (a
        direction off the axis); the fingers wrap round the front from there and the thumb round
        the back. The hand then settles on the grip: turned about its axis, within `turn_range`
        degrees of `knuckles`, and for its kind of `grip` crossing the palm more or less obliquely
        ("power": a handle, a pistol grip; "cradle": a fore-end or a pump lying along the palm), to
        where its arm is most natural; `finish` settles and builds it and reports the hold. Its arm
        runs from the shoulder, its elbow leaning toward `pole` (default out to its side and down),
        in the weapon's held frame (after `finish`'s turn). It rides the part it
        `holds`. The left hand is the right one built at the mirror place and mirrored, so the two
        are the same hand."""
        mirror = side == "l"
        away = ARM_AWAY * (2 if mirror else 1)
        with self.group("hand_%s_upper" % side, parent=None):
            _segment(self, "hand_%s_upper" % side, self.view["upper_arm"], UPPER_R, away, True)
        with self.group("hand_%s_fore" % side, parent=None):
            _segment(self, "hand_%s_fore" % side, self.view["forearm"], FORE_R, away + 0.5,
                     False, cuff=True)
        if pole is None:
            pole = (-1.0 if mirror else 1.0, -0.2, -1.0)
        self.arms.append({"side": side, "holds": holds, "centre": Vector(centre),
                          "axis": Vector(axis).normalized(), "knuckles": Vector(knuckles),
                          "grip_r": grip_r, "index_curl": index_curl, "grip": grip,
                          "pole": Vector(pole), "pole_m": Vector(pole).normalized(),
                          "turn_range": int(turn_range),
                          "away": {"upper": away, "fore": away + 0.5}})

    def _frame_of(self, arm, turn_deg):
        """The right hand's grip frame for `arm` turned `turn_deg` about its axis: at the mirror
        place for a left hand (built as the right one there, then mirrored)."""
        flip = (lambda v: Vector((-v[0], v[1], v[2]))) if arm["side"] == "l" else Vector
        k = Matrix.Rotation(math.radians(turn_deg), 3, arm["axis"]) @ arm["knuckles"]
        return _Frame(flip(arm["centre"]), flip(arm["axis"]), flip(k)), flip

    def _settle_hands(self, m):
        """Each hand settled on its grip, then built: every turn about the grip's axis within its
        range and every obliquity its kind of grip allows tried, judged by its arm at rest from
        its shoulder (`_solve`), the most natural kept. `m` takes the built frame to the held."""
        for arm in self.arms:
            best = None
            for along in WRIST_ALONG[arm["grip"]]:
                for turn in range(-arm["turn_range"], arm["turn_range"] + 1, 3):
                    frame, flip = self._frame_of(arm, turn)
                    w, k = _hand_points(frame, arm["grip_r"], along)
                    w, k = flip(w), flip(k)
                    h = _basis(m @ k - m @ w, m.to_3x3() @ arm["axis"], m @ w)
                    cost = self._solve(arm, h, self.torso_rest)["cost"]
                    if best is None or cost < best[0] - 1e-9:
                        best = (cost, along, turn)
            _, along, turn = best
            arm["turned"], arm["along"] = turn, along
            frame, flip = self._frame_of(arm, turn)
            first = len(self.parts)
            with self.group("hand_" + arm["side"], parent=None):
                w, k = _hand_mesh(self, "hand_" + arm["side"], frame, arm["grip_r"],
                                  arm["index_curl"], along)
            if arm["side"] == "l":
                for o in self.parts[first:]:
                    bm = bmesh.new()
                    bm.from_mesh(o.data)
                    bmesh.ops.scale(bm, vec=(-1, 1, 1), verts=bm.verts)
                    bmesh.ops.reverse_faces(bm, faces=bm.faces)
                    bm.to_mesh(o.data)
                    bm.free()
            arm["wrist"], arm["mid"] = flip(w), flip(k)

    def _body(self, m):
        """The torso at rest (its stance about the shoulders' middle) and the shoulders with it,
        in the held frame."""
        shoulders = list(self.shoulders.values())
        mid = sum(shoulders, Vector((0, 0, 0))) / len(shoulders)
        stance = Matrix.Rotation(-math.radians(self.stance), 4, "Z")
        for side in self.shoulders:
            self.shoulders[side] = mid + stance.to_3x3() @ (self.shoulders[side] - mid)
        self.torso_rest = Matrix.Translation(mid) @ stance

    def mirror_x(self, obj, name):
        """A copy of `obj` mirrored across X = 0 (the other side's part)."""
        me = obj.data.copy()
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.scale(bm, vec=(-1, 1, 1), verts=bm.verts)
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
        bm.to_mesh(me)
        bm.free()
        o = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(o)
        for m in obj.modifiers:
            nm = o.modifiers.new(m.name, m.type)
            for attr in ("width", "segments", "limit_method", "angle_limit", "harden_normals",
                         "profile", "miter_outer", "use_clamp_overlap", "keep_sharp", "weight"):
                if hasattr(m, attr):
                    setattr(nm, attr, getattr(m, attr))
        o["ge_group"] = obj.get("ge_group", "body")
        self.parts.append(o)
        return o

    # ── The end: one mesh, one texture, one file ──

    def finish(self, name, out, turn=None):
        """Joins the parts into one mesh named `name`, unwraps and bakes it, splits it into its
        nodes, poses the arms, keys the clips and writes `out`. `turn` (a Matrix) turns the whole
        weapon about the origin after the bake (the texture stays where the parts were): how it
        is held. The grip hand's hold (the right hand's grip, else the origin) becomes the
        origin."""
        m = turn.copy() if turn is not None else Matrix.Identity(4)
        grip = [a for a in self.arms if a["side"] == "r"]
        hold = m @ grip[0]["centre"] if grip else Vector((0, 0, 0))
        m = Matrix.Translation(-hold) @ m
        self._body(m)
        self._settle_hands(m)
        obj = self._join(name)
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        verts = len(obj.data.vertices)
        self._unwrap(obj)
        colour, glow = self._bake(obj)
        self._final_material(obj, name, colour, glow)
        nodes = self._split(obj, name)
        self._rest(nodes, name, m)
        self._animate(nodes)
        print("weapon-kit: %s: %d triangles, %d vertices, %d nodes, %d clips" %
              (name, tris, verts, len(nodes), len(self.clips)))
        ge_asset.export(out, textures=True, animations=bool(self.clips))

    def _split(self, obj, name):
        """Each group's faces split off into an object of its own (the body keeps the rest)."""
        nodes = {"body": obj}
        for gi, g in enumerate(self.order):
            if g == "body":
                continue
            bpy.ops.object.mode_set(mode="OBJECT")
            vals = [0] * len(obj.data.polygons)
            obj.data.attributes["ge_group"].data.foreach_get("value", vals)
            if gi not in vals:
                continue
            # Only this group's faces, vertices and edges selected (the unwrap left all selected).
            me = obj.data
            me.vertices.foreach_set("select", [False] * len(me.vertices))
            me.edges.foreach_set("select", [False] * len(me.edges))
            for p, v in zip(me.polygons, vals):
                p.select = v == gi
                if p.select:
                    for i in p.vertices:
                        me.vertices[i].select = True
            for e in me.edges:
                e.select = me.vertices[e.vertices[0]].select and me.vertices[e.vertices[1]].select
            bpy.context.view_layer.objects.active = obj
            for o in bpy.context.scene.objects:
                o.select_set(o == obj)
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.select_mode(type="FACE")
            bpy.ops.mesh.separate(type="SELECTED")
            bpy.ops.object.mode_set(mode="OBJECT")
            part = [o for o in bpy.context.selected_objects if o != obj][0]
            part.name = name + "." + g
            part.data.name = part.name
            nodes[g] = part
        for o in nodes.values():
            if "ge_group" in o.data.attributes:
                o.data.attributes.remove(o.data.attributes["ge_group"])
        return nodes

    def _rest(self, nodes, name, m):
        """Every node's rest: the weapon's parts turned by `m` with their origins at their pivots;
        a torso at the shoulders; each arm posed between its shoulder and its hand. Records the
        world matrices and how each hand sits on what it holds, for `_animate`."""
        self.world = {}
        self.parent = {}
        for g, o in nodes.items():
            if g.startswith("hand_") and ("_upper" in g or "_fore" in g):
                continue
            o.data.transform(m)
            if g.startswith("hand_"):
                continue
            pivot = self.groups[g]["pivot"]
            if isinstance(pivot, str):  # "shoulder_r": the shoulder, in the held frame already
                pv = self.shoulders[pivot[-1]].copy()
            else:
                pv = m @ Vector(pivot)
            o.data.transform(Matrix.Translation(-pv))
            self.world[g] = Matrix.Translation(pv)
            self.parent[g] = self.groups[g]["parent"]
        torso = bpy.data.objects.new(name + ".torso", None)
        bpy.context.scene.collection.objects.link(torso)
        nodes["torso"] = torso
        self.world["torso"] = self.torso_rest
        self.parent["torso"] = None
        self.hold = {}
        for arm in self.arms:
            s = arm["side"]
            hand = "hand_" + s
            w = m @ arm["wrist"]
            k = m @ arm["mid"]
            a = (m.to_3x3() @ arm["axis"]).normalized()
            h = _basis(k - w, a, w)
            nodes[hand].data.transform(h.inverted())
            for seg in ("upper", "fore"):
                nodes["%s_%s" % (hand, seg)].data.transform(
                    Matrix.Translation((-arm["away"][seg], 0, 0)))
            self.hold[s] = self.world[arm["holds"]].inverted() @ h
            self.parent[hand + "_upper"] = "torso"
            self.parent[hand + "_fore"] = hand + "_upper"
            self.parent[hand] = hand + "_fore"
        worlds = self._pose(self.world)
        for arm in self.arms:
            a = arm["last"]
            print("weapon-kit: the %s hand settled %+d deg round its grip, the wrist %.3f along it; "
                  "the arm at rest: elbow %.0f deg bent pointing %.0f (0 down, 90 out), wrist %.0f "
                  "back %.0f thumbward, twist %.0f, shoulder up %.0f swing %.0f%s" %
                  (arm["side"], arm["turned"], arm["along"], a["elbow_flex"], a["elbow_roll"], a["wrist_extension"],
                   a["wrist_radial"], a["forearm_twist"], a["shoulder_elevation"],
                   a["shoulder_yaw"], "" if "over" not in arm else
                   "  OUT OF RANGE by %.0f" % arm["over"]))
        for g, o in nodes.items():
            o.rotation_mode = "QUATERNION"
            p = self.parent[g]
            if p is not None:
                o.parent = nodes[p]
            o.matrix_parent_inverse = Matrix.Identity(4)
            o.matrix_basis = (worlds[p].inverted() @ worlds[g]) if p is not None else worlds[g]
        self.rest_world = worlds

    def _solve(self, arm, h, torso, prev=None):
        """Arm `arm`'s elbow for the hand at `h` with the body at `torso` (its world): the place on
        the elbow's circle inside the arm's ranges (viewmodel/arm.hpp) with the most natural arm,
        near `prev` (the last frame's angle). Returns the shoulder, the elbow, its angle, how far
        past its reach, the angles and the score."""
        s = arm["side"]
        torso0 = self.torso_rest
        sh = torso @ torso0.inverted() @ self.shoulders[s]
        w = h.translation
        l1, l2 = self.view["upper_arm"], self.view["forearm"]
        turn = torso.to_3x3()  # the body's own axes: its stance and its motion
        right, up, fwd = (_view(turn @ Vector(v)) for v in ((1, 0, 0), (0, 0, 1), (0, 1, 0)))
        distal = _view(h.to_3x3() @ Vector((0, 1, 0)))
        radial = _view(h.to_3x3() @ Vector((1, 0, 0)))
        side = 1.0 if s == "r" else -1.0
        lim = self.view.get("limits", LIMITS)
        pole = (torso.to_3x3() @ torso0.to_3x3().inverted() @ arm["pole_m"]).normalized()

        def angles(e):
            return _arm_angles(_view(sh), _view(e), _view(w), distal, radial, right, up, fwd, side)

        def judge(e):
            ang = angles(e)
            lean = (e - sh).normalized().dot(pole)
            # Inside the ranges first; then comfort: each joint free inside its easy band and
            # dearer the further it strays toward its limit, so no one joint is run to its end to
            # spare another.
            cost = 50.0 * _excess(ang, lim) - 4.0 * lean
            for key, lo, hi, weight in COMFORT:
                d = max(0.0, lo - ang[key], ang[key] - hi)
                cost += weight * (d * 0.1) ** 2
            return cost

        e, theta, short, cost = _ik(sh, w, l1, l2, judge, prev)
        return {"sh": sh, "e": e, "theta": theta, "short": short, "ang": angles(e),
                "cost": cost + 200.0 * short, "pole": pole, "lim": lim}

    def _pose(self, world):
        """The arms' worlds added to `world` (the weapon's and the torso's): each hand on what it
        holds, its elbow by IK from its shoulder."""
        out = dict(world)
        for arm in self.arms:
            s = arm["side"]
            hand = "hand_" + s
            h = out[arm["holds"]] @ self.hold[s]
            r = self._solve(arm, h, out["torso"], arm.get("theta"))
            arm["theta"] = r["theta"]
            if r["short"] > 0.003:
                arm["short"] = max(arm.get("short", 0.0), r["short"])
            ang = r["ang"]
            arm["bend"] = max(arm.get("bend", 0.0), abs(ang["wrist_extension"]),
                              abs(ang["wrist_radial"]))
            over = _excess(ang, r["lim"])
            if over > 0.5:
                arm["over"] = max(arm.get("over", 0.0), over)
            arm["last"] = ang
            sh, e, w = r["sh"], r["e"], h.translation
            hand_x = h.to_3x3() @ Vector((1, 0, 0))
            out[hand + "_upper"] = _basis(e - sh, r["pole"].cross(e - sh), sh)
            out[hand + "_fore"] = _basis(w - e, hand_x, e)
            out[hand] = h
        return out

    def _animate(self, nodes):
        """Each clip keyed on every node at every frame (60 a second): the weapon's parts by their
        keys, smoothstepped; the arms by IK; pushed down as an NLA track of the clip's name."""
        scene = bpy.context.scene
        scene.render.fps = 60
        rest_theta = {a["side"]: a.get("theta") for a in self.arms}
        for arm in self.arms:
            arm.pop("short", None)
            arm.pop("bend", None)
            arm.pop("over", None)
        order = []  # parents before children
        while len(order) < len(self.world):
            for g in self.world:
                if g not in order and (self.parent[g] is None or self.parent[g] in order):
                    order.append(g)
        for name, keys, ease in self.clips:
            for g in keys:
                if g not in self.world:
                    raise SystemExit("weapon_kit: clip %s keys %s, which is no part" % (name, g))
            last = max(k[-1][0] for k in keys.values())
            frames = int(round(last * 60))
            for arm in self.arms:
                arm["theta"] = rest_theta[arm["side"]]
            prev = {}
            for f in range(frames + 1):
                t = f / 60
                world = {}
                for g in order:
                    rest = self.world[g]
                    p = self.parent[g]
                    local = (self.world[p].inverted() @ rest) if p is not None else rest
                    if g in keys:
                        d, r = _ease(keys[g], t, ease == "linear")
                        # Offset and turned in the parent's axes, about the part's own pivot, on
                        # top of its rest turn (a torso's stance).
                        local = (Matrix.Translation(local.translation + d) @
                                 Euler(r, "XYZ").to_matrix().to_4x4() @
                                 local.to_3x3().to_4x4())
                    world[g] = (world[p] @ local) if p is not None else local
                worlds = self._pose(world)
                for g, o in nodes.items():
                    p = self.parent[g]
                    basis = (worlds[p].inverted() @ worlds[g]) if p is not None else worlds[g]
                    loc, rot, _ = basis.decompose()
                    if g in prev:
                        rot.make_compatible(prev[g])
                    prev[g] = rot
                    o.location = loc
                    o.rotation_quaternion = rot
                    o.keyframe_insert(data_path="location", frame=f)
                    o.keyframe_insert(data_path="rotation_quaternion", frame=f)
            for g, o in nodes.items():
                action = o.animation_data.action
                action.name = name + "." + g
                for fc in _fcurves(action):
                    for kp in fc.keyframe_points:
                        kp.interpolation = "LINEAR"
                track = o.animation_data.nla_tracks.new()
                track.name = name
                strip = track.strips.new(name, 0, action)
                strip.name = name
                o.animation_data.action = None
        for g, o in nodes.items():
            p = self.parent[g]
            w = self.rest_world
            o.matrix_basis = (w[p].inverted() @ w[g]) if p is not None else w[g]
        for arm in self.arms:
            print("weapon-kit: the %s wrist bends %.0f deg at worst (forearm to hand)" %
                  (arm["side"], arm.get("bend", 0.0)))
            if "over" in arm:
                print("weapon-kit: the %s arm leaves its ranges by %.0f deg at worst: move the "
                      "grip or the hold (the lab names the joint)" % (arm["side"], arm["over"]))
            if "short" in arm:
                print("weapon-kit: the %s hand is %.3f m past the arm's reach at worst: move the "
                      "grip nearer the shoulder or the offset in weapons.cpp" %
                      (arm["side"], arm["short"]))
        scene.frame_set(0)

    def _join(self, name):
        dg = bpy.context.evaluated_depsgraph_get()
        meshes = []
        groups = [p.get("ge_group", "body") for p in self.parts]
        self.order = ["body"] + [g for g in self.groups if g != "body"]
        for p in self.parts:
            me = bpy.data.meshes.new_from_object(p.evaluated_get(dg), preserve_all_data_layers=True,
                                                 depsgraph=dg)
            me.transform(p.matrix_world)
            meshes.append((me, p.data.materials[0]))
        for p in self.parts:
            bpy.data.objects.remove(p)
        target = bpy.data.meshes.new(name)
        obj = bpy.data.objects.new(name, target)
        bpy.context.scene.collection.objects.link(obj)
        bm = bmesh.new()
        mats = []
        face_groups = []
        for (me, mat), p_group in zip(meshes, groups):
            face_groups.extend([self.order.index(p_group)] * len(me.polygons))
        for me, mat in meshes:
            if mat.name not in [m.name for m in mats]:
                mats.append(mat)
            index = [m.name for m in mats].index(mat.name)
            for p in me.polygons:
                p.material_index = index
            bm.from_mesh(me)
        # Custom normals do not survive bmesh: rebuild them from the parts' own after.
        bm.to_mesh(target)
        bm.free()
        for m in mats:
            target.materials.append(m)
        attr = target.attributes.new("ge_group", "INT", "FACE")
        attr.data.foreach_set("value", face_groups)
        # The normals: each part's own (hardened by its bevel), copied loop for loop.
        normals = []
        for me, _ in meshes:
            for loop in me.loops:
                normals.append(tuple(me.corner_normals[loop.index].vector))
        target.normals_split_custom_set(normals)
        for me, _ in meshes:
            bpy.data.meshes.remove(me)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        return obj

    def _unwrap(self, obj):
        """Islands cut by angle (smart project), then packed by `_pack` into the 2:1 picture."""
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.0,
                                 area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        self._pack(obj)

    @staticmethod
    def _pack(obj, pad_px=6):
        """The islands shelf-packed into a box of TEX_W x TEX_H texels: each turned to lie wider
        than tall, the tallest first, rows filled left to right, at the largest scale that fits
        (found by halving), `pad_px` apart. Texels stay square, so a pattern keeps its shape."""
        from bpy_extras import bmesh_utils

        bm = bmesh.new()
        bm.from_mesh(obj.data)
        uv = bm.loops.layers.uv.active
        islands = bmesh_utils.bmesh_linked_uv_islands(bm, uv)
        boxes = []
        for faces in islands:
            pts = [l[uv].uv for f in faces for l in f.loops]
            x0 = min(p.x for p in pts)
            y0 = min(p.y for p in pts)
            w = max(p.x for p in pts) - x0
            h = max(p.y for p in pts) - y0
            boxes.append({"faces": faces, "x0": x0, "y0": y0, "w": w, "h": h, "turn": h > w})
        order = sorted(range(len(boxes)), key=lambda i: -min(boxes[i]["w"], boxes[i]["h"]))

        def place(scale):
            at = {}
            x = y = row = 0.0
            for i in order:
                b = boxes[i]
                w = (b["h"] if b["turn"] else b["w"]) * scale + pad_px
                h = (b["w"] if b["turn"] else b["h"]) * scale + pad_px
                if x + w > TEX_W:
                    x, y, row = 0.0, y + row, 0.0
                if x + w > TEX_W or y + h > TEX_H:
                    return None
                at[i] = (x + pad_px / 2, y + pad_px / 2)
                x += w
                row = max(row, h)
            return at

        lo, hi = 1.0, 1e5
        for _ in range(40):
            mid = (lo + hi) / 2
            if place(mid) is None:
                hi = mid
            else:
                lo = mid
        at = place(lo)
        for i, b in enumerate(boxes):
            ox, oy = at[i]
            for f in b["faces"]:
                for l in f.loops:
                    p = l[uv].uv
                    dx, dy = (p.x - b["x0"]) * lo, (p.y - b["y0"]) * lo
                    if b["turn"]:
                        dx, dy = (p.y - b["y0"]) * lo, (b["x0"] + b["w"] - p.x) * lo
                    l[uv].uv = ((ox + dx) / TEX_W, (oy + dy) / TEX_H)
        bm.to_mesh(obj.data)
        bm.free()
        filled = sum(b["w"] * b["h"] for b in boxes) * lo * lo / (TEX_W * TEX_H)
        print("weapon-kit: %d islands fill %.0f%% of the texture" % (len(boxes), 100 * filled))

    def _bake(self, obj):
        scene = bpy.context.scene
        scene.render.engine = "CYCLES"
        scene.cycles.device = "CPU"
        scene.cycles.samples = BAKE_SAMPLES
        scene.cycles.use_denoising = False
        scene.cycles.seed = 0
        scene.render.bake.margin = 8
        scene.render.bake.margin_type = "EXTEND"
        scene.render.bake.use_clear = True
        mats = list(obj.data.materials)
        glows = any(m.get("ge_glows") for m in mats)

        def bake(kind):
            img = bpy.data.images.new(obj.name + "_" + kind, TEX_W, TEX_H, alpha=False)
            img.colorspace_settings.name = "sRGB"
            for m in mats:
                nt = m.node_tree
                t = nt.nodes["bake_target"]
                t.image = img
                nt.nodes.active = t
                src = nt.nodes[m["ge_colour"] if kind == "colour" else m["ge_glow"]]
                out = [n for n in nt.nodes if n.bl_idname == "ShaderNodeOutputMaterial"][0]
                nt.links.new(src.outputs[0], out.inputs["Surface"])
            bpy.ops.object.bake(type="EMIT")
            img.pack()
            return img

        colour = bake("colour")
        glow = bake("glow") if glows else None
        return colour, glow

    def _final_material(self, obj, name, colour, glow):
        mat = bpy.data.materials.new(name)
        nt = mat.node_tree
        bsdf = nt.nodes.get("Principled BSDF")
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = colour
        tex.interpolation = "Closest"
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.45
        bsdf.inputs["Metallic"].default_value = 0.0
        if glow is not None:
            g = nt.nodes.new("ShaderNodeTexImage")
            g.image = glow
            g.interpolation = "Closest"
            nt.links.new(g.outputs["Color"], bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = 1.0
        obj.data.materials.clear()
        obj.data.materials.append(mat)
        for p in obj.data.polygons:
            p.material_index = 0
