# The skier model's builder: the skier on the reference pair with his poles, rigged on the bones
# of app/rig.hpp and skinned by hand-set weights, made off the `skier` data set (data.cpp) in
# headless Blender by `ge asset build skier`. Low-poly on purpose: a few thousand triangles, one
# mesh, one material a garment, read at chase range.
#
# Every limb is lofted round its bone at bind (each runs along -y; rig.hpp), its weights blended
# across a joint over BLEND metres either side, half and half at the joint itself, so a bend
# folds the sleeve or the trouser leg rather than tearing it. The boots, the bindings and the
# skis are rigid on the ankles; the gloves on the wrists; the helmet and the goggles on the head;
# each pole on its own bone.
#
# ge:summary The skier model's Blender builder: the rigged figure, the skis and the poles
# ge:keywords skier model builder blender rig skin weights loft skis poles helmet boots
# ge:skill skier assets

import math

import ge_asset

BLEND = 0.06  # m either side of a joint the skin is shared over

COLOURS = {
    "jacket": (0.62, 0.05, 0.04),
    "pants": (0.02, 0.025, 0.04),
    "helmet": (0.85, 0.86, 0.88),
    "goggle": (0.95, 0.42, 0.02),
    "glove": (0.015, 0.015, 0.018),
    "boot": (0.04, 0.05, 0.06),
    "binding": (0.35, 0.36, 0.38),
    "ski": (0.9, 0.62, 0.02),
    "pole": (0.55, 0.56, 0.6),
}


def smooth(a, b, x):
    """0 at a, 1 at b, smooth between (a > b allowed)."""
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def chain(y, bones):
    """The weights of a point at height y on a limb down `bones`: [(joint, y_top, y_bottom)], each
    joint's bone from its top to its bottom, top to bottom. Shared half and half at each joint."""
    w = {bones[0][0]: 1.0}
    for i in range(1, len(bones)):
        upper, lower = bones[i - 1][0], bones[i][0]
        at = bones[i][1]
        share = smooth(at + BLEND, at - BLEND, y)
        moved = w.get(upper, 0.0) * share
        if moved > 0:
            w[upper] -= moved
            w[lower] = w.get(lower, 0.0) + moved
    return {k: v for k, v in w.items() if v > 1e-4}


def at(joints, name):
    return next(j for j in joints if j["name"] == name)["at"]


def lerp(a, b, t):
    return [a[k] + (b[k] - a[k]) * t for k in range(3)]


def build_legs(mesh, joints):
    for side in ("l", "r"):
        hip, knee, ankle = (at(joints, n + "_" + side) for n in ("hip", "knee", "ankle"))
        bones = [("hip_" + side, hip[1], knee[1]), ("knee_" + side, knee[1], ankle[1]),
                 ("ankle_" + side, ankle[1], 0)]
        rings = []
        for y, r in ((1.07, 0.088), (0.98, 0.083), (0.86, 0.075), (0.72, 0.066), (0.64, 0.061),
                     (0.56, 0.06), (0.47, 0.062), (0.38, 0.056), (0.28, 0.05)):
            t = (hip[1] - y) / (hip[1] - ankle[1])
            c = lerp(hip, ankle, max(0.0, min(1.0, t)))
            c[1] = y
            rings.append((c, r, r * 1.05, chain(y, bones)))
        mesh.loft(rings, "pants")
        # The boot: its cuff round the shin, its shell over the foot, rigid on the ankle.
        w = {"ankle_" + side: 1.0}
        x = ankle[0]
        mesh.box((x, 0.21, -0.01), (0.12, 0.2, 0.17), "boot", w)
        mesh.box((x, 0.085, 0.02), (0.11, 0.09, 0.31), "boot", w)
        mesh.box((x, 0.029, 0.0), (0.075, 0.022, 0.42), "binding", w)


def ski_rocker(d, rocker_length, rise):
    """How far a ski stands off the snow `d` metres in from its end."""
    if d >= rocker_length:
        return 0.0
    u = 1 - d / rocker_length
    return rise * u * u


def build_skis(mesh, joints, skis):
    length = skis["length"]
    half_waist = skis["waist"] / 2
    radius = skis["sidecut"]
    mount = skis["mount_back"]
    thick = skis["thickness"]
    steps = 36
    for side in ("l", "r"):
        x = at(joints, "ankle_" + side)[0]
        w = {"ankle_" + side: 1.0}
        tail = mount - length / 2
        rows = []
        for i in range(steps + 1):
            z = tail + length * i / steps
            from_waist = z - mount
            half = half_waist + from_waist * from_waist / (2 * radius)
            d = min(i, steps - i) * length / steps
            # The tip and the tail round off over their last few stations.
            end = min(1.0, 0.45 + 0.55 * math.sqrt(min(1.0, d / 0.06)))
            base = ski_rocker(d, skis["rocker_length"], skis["rocker_rise"])
            rows.append([
                mesh.vertex((x - half * end, base, z), w),
                mesh.vertex((x + half * end, base, z), w),
                mesh.vertex((x + half * end, base + thick, z), w),
                mesh.vertex((x - half * end, base + thick, z), w),
            ])
        for i in range(steps):
            a, b = rows[i], rows[i + 1]
            for k in range(4):
                n = (k + 1) % 4
                mesh.face((a[k], a[n], b[n], b[k]), "ski", smooth=False)
        mesh.face(rows[0], "ski", smooth=False)
        mesh.face(rows[-1], "ski", smooth=False)


def build_torso(mesh, joints):
    pelvis, spine, chest, neck, head = (at(joints, n)
                                        for n in ("pelvis", "spine", "chest", "neck", "head"))
    bones = [("pelvis", pelvis[1], spine[1]), ("spine", spine[1], chest[1]),
             ("chest", chest[1], neck[1]), ("neck", neck[1], head[1])]

    def rings(spec):
        out = []
        for y, rx, rz in spec:
            out.append(((0.0, y, 0.0), rx, rz, chain(-y, [(b, -t, -u) for b, t, u in bones])))
        return out

    # chain() walks a limb downward, so the torso is measured as depth below the top (-y).
    mesh.loft(rings(((0.93, 0.16, 0.11), (1.0, 0.168, 0.115), (1.08, 0.165, 0.112))), "pants")
    mesh.loft(rings(((1.03, 0.172, 0.122), (1.12, 0.16, 0.115), (1.22, 0.162, 0.118),
                     (1.32, 0.178, 0.124), (1.42, 0.19, 0.125), (1.49, 0.165, 0.11),
                     (1.53, 0.075, 0.072), (1.58, 0.06, 0.06))), "jacket", sides=12)
    w = {"head": 1.0}
    mesh.sphere((0.0, head[1] + 0.065, 0.005), 0.125, "helmet", w, squash=(0.92, 1.0, 1.05))
    mesh.box((0.0, head[1] + 0.06, 0.105), (0.17, 0.065, 0.05), "goggle", w, smooth=True)


def build_arms(mesh, joints):
    for side in ("l", "r"):
        shoulder, elbow, wrist = (at(joints, n + "_" + side) for n in ("shoulder", "elbow", "wrist"))
        bones = [("chest", 1.6, shoulder[1]), ("shoulder_" + side, shoulder[1], elbow[1]),
                 ("elbow_" + side, elbow[1], wrist[1])]
        rings = []
        for y, r in ((1.5, 0.06), (1.44, 0.058), (1.34, 0.054), (1.24, 0.05), (1.17, 0.047),
                     (1.08, 0.045), (0.99, 0.042)):
            t = (shoulder[1] - y) / (shoulder[1] - wrist[1])
            c = lerp(shoulder, wrist, max(0.0, min(1.0, t)))
            c[1] = y
            weights = chain(y, bones)
            if y > shoulder[1]:
                weights = {"chest": 0.5, "shoulder_" + side: 0.5}
            rings.append((c, r, r, weights))
        mesh.loft(rings, "jacket", sides=8)
        mesh.box((wrist[0], wrist[1] - 0.045, 0.0), (0.075, 0.11, 0.095), "glove",
                 {"wrist_" + side: 1.0}, smooth=True)


def build_poles(mesh, joints, poles):
    for side in ("l", "r"):
        grip = at(joints, "pole_" + side)
        tip = at(joints, "pole_tip_" + side)
        w = {"pole_" + side: 1.0}
        top = (grip[0], grip[1] + 0.08, grip[2])
        mesh.loft(((top, 0.011, 0.011, w), (tip, 0.006, 0.006, w)), "pole", sides=6)
        basket = (tip[0], tip[1] + 0.09, tip[2])
        mesh.loft(((basket, 0.045, 0.045, w), ((basket[0], basket[1] + 0.012, basket[2]), 0.045,
                                                0.045, w)), "pole", sides=8)


def main():
    data, out = ge_asset.start()
    joints = data["joints"]
    rig = ge_asset.armature("skier_rig", joints)
    mats = {name: ge_asset.material(name, rgb) for name, rgb in COLOURS.items()}
    mesh = ge_asset.MeshBuilder()
    build_skis(mesh, joints, data["skis"])
    build_legs(mesh, joints)
    build_torso(mesh, joints)
    build_arms(mesh, joints)
    build_poles(mesh, joints, data["poles"])
    mesh.finish("skier", rig, mats)
    ge_asset.export(out)


main()
