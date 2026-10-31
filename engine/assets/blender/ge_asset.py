# ge_asset: the library every model builder imports, run inside headless Blender by `ge asset
# build` (engine/assets/manifest.hpp; skill: assets). It is compiled into `ge` and written beside
# the build, so a game repo's builders get the engine's own copy.
#
# A builder is a Blender Python script that makes one model from the game's data and exports it:
#
#     import ge_asset
#     data, out = ge_asset.start()           # the data set's JSON (or None) and the .glb's path
#     rig = ge_asset.armature("rig", joints)  # bones from the game's joints
#     mesh = ge_asset.MeshBuilder()
#     mesh.box(centre, size, "boot", {"ankle_l": 1})
#     mesh.finish("skier", rig)
#     ge_asset.export(out)
#
# THE FRAME. A builder works in the ENGINE's frame (x right, y up, z forward, metres), the frame
# the game's data is in; `to_blender` turns a point into Blender's (z up, the front toward -y,
# the right toward -x: the usual front view), and the glTF exporter and the engine's loader turn
# it back. So a model built here faces heading 0 in the game.
#
# THE RULES the export keeps (the assets skill's): normals exported, smooth where a builder asks;
# at most four influences a vertex, normalised; base colours, and textures only when a builder
# bakes its own and asks; no cameras or lights; actions sampled at 60 frames a second. A builder never reads a clock or a random seed of its
# own: the same data gives the same model.
#
# ge:summary The library a Blender model builder imports: arguments, the frame, bones, meshes, export
# ge:keywords assets blender bpy builder ge-asset armature mesh-builder export glb frame to_blender
# ge:skill assets

import json
import sys

import bpy  # first: in the bpy module, bmesh exists only once bpy is loaded
import bmesh  # noqa: E402

VERSION = 1


def start():
    """Clears the scene and returns (data, out): the data set's JSON as a dict (None when the
    manifest names none) and the path the .glb is written to."""
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    opts = {}
    for i in range(0, len(argv) - 1, 2):
        opts[argv[i].lstrip("-")] = argv[i + 1]
    if "out" not in opts:
        raise SystemExit("ge_asset: no --out <path>; run the builder through `ge asset build`")
    data = None
    if "data" in opts:
        with open(opts["data"], encoding="utf-8") as f:
            data = json.load(f)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = 60
    scene.unit_settings.system = "METRIC"
    print("ge-asset: blender " + bpy.app.version_string)
    return data, opts["out"]


def to_blender(p):
    """A point or direction in the engine's frame, in Blender's."""
    return (-p[0], -p[2], p[1])


def material(name, rgb, roughness=0.6, metallic=0.0):
    """A material of one base colour (linear RGB), reused by name."""
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
        mat.diffuse_color = (rgb[0], rgb[1], rgb[2], 1.0)
    return mat


def armature(name, joints, leaf=0.05):
    """An armature with one bone a joint, named after it: its head at the joint, its tail at the
    joint's first child (or `leaf` metres along -y for a joint with none), parented as the joints
    are. `joints` is a list of {"name", "parent", "at"} in the engine's frame, parents first."""
    arm = bpy.data.armatures.new(name)
    obj = bpy.data.objects.new(name, arm)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    child = {}
    for i, j in enumerate(joints):
        if j["parent"] >= 0 and j["parent"] not in child:
            child[j["parent"]] = i
    bones = []
    for i, j in enumerate(joints):
        b = arm.edit_bones.new(j["name"])
        head = j["at"]
        if i in child:
            tail = joints[child[i]]["at"]
        else:
            tail = (head[0], head[1] - leaf, head[2])
        if sum((tail[k] - head[k]) ** 2 for k in range(3)) < 1e-8:
            tail = (head[0], head[1] - leaf, head[2])
        b.head = to_blender(head)
        b.tail = to_blender(tail)
        b.roll = 0.0
        if j["parent"] >= 0:
            b.parent = bones[j["parent"]]
            b.use_connect = False
        bones.append(b)
    bpy.ops.object.mode_set(mode="OBJECT")
    return obj


class MeshBuilder:
    """Vertices, quads and triangles in the engine's frame, each with a material and the joints
    it is bound to ({joint: weight}), made into one skinned mesh by `finish`."""

    def __init__(self):
        self.verts = []
        self.weights = []
        self.faces = []
        self.mats = []
        self.smooth = []
        self.names = []

    def _mat(self, name):
        if name not in self.names:
            self.names.append(name)
        return self.names.index(name)

    def vertex(self, p, weights):
        self.verts.append(to_blender(p))
        total = sum(weights.values())
        self.weights.append({k: w / total for k, w in weights.items() if w > 0})
        return len(self.verts) - 1

    def face(self, idx, mat, smooth=True):
        self.faces.append(tuple(idx))
        self.mats.append(self._mat(mat))
        self.smooth.append(smooth)

    def loft(self, rings, mat, sides=10, caps=True):
        """A tube through `rings`, each (centre, rx, rz, weights): an ellipse across x and z
        round its centre. Closed at both ends when `caps`."""
        import math

        rows = []
        for centre, rx, rz, w in rings:
            row = []
            for k in range(sides):
                a = 2 * math.pi * k / sides
                p = (centre[0] + rx * math.cos(a), centre[1], centre[2] + rz * math.sin(a))
                row.append(self.vertex(p, w))
            rows.append(row)
        for r in range(len(rows) - 1):
            for k in range(sides):
                n = (k + 1) % sides
                self.face((rows[r][k], rows[r][n], rows[r + 1][n], rows[r + 1][k]), mat)
        if caps:
            for row, (centre, _, _, w) in ((rows[0], rings[0]), (rows[-1], rings[-1])):
                c = self.vertex(centre, w)
                for k in range(sides):
                    self.face((row[k], row[(k + 1) % sides], c), mat)

    def box(self, centre, size, mat, weights, smooth=False):
        """A box of `size` (x, y, z) round `centre`, flat-shaded."""
        h = [s / 2 for s in size]
        corner = []
        for i in range(8):
            p = (centre[0] + (h[0] if i & 1 else -h[0]), centre[1] + (h[1] if i & 2 else -h[1]),
                 centre[2] + (h[2] if i & 4 else -h[2]))
            corner.append(p)
        for quad in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4),
                     (1, 5, 7, 3)):
            self.face([self.vertex(corner[q], weights) for q in quad], mat, smooth)

    def sphere(self, centre, r, mat, weights, rings=8, sides=12, squash=(1, 1, 1)):
        """A UV sphere of radius `r`, scaled by `squash` on x, y and z."""
        import math

        top = self.vertex((centre[0], centre[1] + r * squash[1], centre[2]), weights)
        rows = []
        for i in range(1, rings):
            t = math.pi * i / rings
            row = []
            for k in range(sides):
                a = 2 * math.pi * k / sides
                p = (centre[0] + r * squash[0] * math.sin(t) * math.cos(a),
                     centre[1] + r * squash[1] * math.cos(t),
                     centre[2] + r * squash[2] * math.sin(t) * math.sin(a))
                row.append(self.vertex(p, weights))
            rows.append(row)
        bottom = self.vertex((centre[0], centre[1] - r * squash[1], centre[2]), weights)
        for k in range(sides):
            n = (k + 1) % sides
            self.face((top, rows[0][n], rows[0][k]), mat)
            self.face((bottom, rows[-1][k], rows[-1][n]), mat)
        for i in range(len(rows) - 1):
            for k in range(sides):
                n = (k + 1) % sides
                self.face((rows[i][k], rows[i][n], rows[i + 1][n], rows[i + 1][k]), mat)

    def finish(self, name, rig, materials):
        """The mesh object, its faces turned outward, skinned to `rig`'s bones by the weights,
        its materials from `materials` ({name: material})."""
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.verts, [], self.faces)
        for name_ in self.names:
            me.materials.append(materials[name_])
        for poly, m, s in zip(me.polygons, self.mats, self.smooth):
            poly.material_index = m
            poly.use_smooth = s
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bmesh.ops.triangulate(bm, faces=bm.faces)
        bm.to_mesh(me)
        bm.free()
        me.update()
        obj = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(obj)
        obj.parent = rig
        groups = {}
        for v, w in enumerate(self.weights):
            for joint, weight in w.items():
                if joint not in groups:
                    groups[joint] = obj.vertex_groups.new(name=joint)
                groups[joint].add([v], weight, "REPLACE")
        mod = obj.modifiers.new("rig", "ARMATURE")
        mod.object = rig
        return obj


def export(out, textures=False, animations=False):
    """The scene as a binary glTF: the skins, the normals and the base colours; no cameras or
    lights. `textures`: the materials' pictures and the texture coordinates too (a baked texture
    the game keeps whole). `animations`: every NLA track as one animation named after it (a clip
    spread over several objects is one track name on each), sampled at the scene's 60 frames a
    second."""
    bpy.ops.export_scene.gltf(
        filepath=out,
        export_format="GLB",
        export_yup=True,
        export_skins=True,
        export_animations=animations,
        export_animation_mode="NLA_TRACKS",
        export_force_sampling=True,
        export_frame_step=1,
        export_optimize_animation_size=False,
        export_cameras=False,
        export_lights=False,
        export_image_format="AUTO" if textures else "NONE",
        export_texcoords=textures,
        export_tangents=False,
        export_def_bones=False,
    )
