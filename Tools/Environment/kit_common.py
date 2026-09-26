# Blender helpers shared by the generated low-poly kits (graveyard_kit.py): a mesh builder that stores colours in
# vertex colours (alpha = sway weight for M_ForestKit) and puts faces in material slot 0 (lit) or 1 (glowing),
# plus a few primitives (boxes, extruded outlines, tubes, lumps). Everything is built in metres, Z up, front -Y.
import bpy, bmesh, math, os, random
from mathutils import Matrix, Vector, noise


def mix(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def shade(c, f):
    return (c[0] * f, c[1] * f, c[2] * f)


def place(x=0.0, y=0.0, z=0.0, yaw=0.0, pitch=0.0, roll=0.0):
    """A placement matrix: rotate (degrees; roll about X, pitch about Y, then yaw about Z) and move."""
    return (Matrix.Translation((x, y, z)) @ Matrix.Rotation(math.radians(yaw), 4, "Z")
            @ Matrix.Rotation(math.radians(pitch), 4, "Y") @ Matrix.Rotation(math.radians(roll), 4, "X"))


I = Matrix.Identity(4)


class Piece:
    """Accumulates vertices (with RGBA colour) and faces (with material slot) for one exported mesh."""

    def __init__(self, name, seed):
        self.name, self.v, self.c, self.f, self.m = name, [], [], [], []
        self.rng = random.Random(seed)

    def vert(self, p, color, sway=0.0):
        self.v.append(Vector(p))
        self.c.append((color[0], color[1], color[2], sway))
        return len(self.v) - 1

    def face(self, idx, slot=0):
        self.f.append(tuple(idx))
        self.m.append(slot)

    def box(self, M, size, color, slot=0, jitter=0.0, taper=1.0, sway=0.0):
        """A box centred on M's origin. color(local position, local normal) -> rgb. taper scales the top face."""
        sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
        rng = self.rng
        ids = []
        for z in (-sz, sz):
            t = taper if z > 0 else 1.0
            for x, y in ((-sx, -sy), (sx, -sy), (sx, sy), (-sx, sy)):
                p = Vector((x * t + rng.uniform(-jitter, jitter), y * t + rng.uniform(-jitter, jitter), z + rng.uniform(-jitter, jitter)))
                ids.append(self.vert(M @ p, color(p, p.normalized()), sway))
        for f in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            self.face([ids[i] for i in f], slot)
        return ids

    def slab(self, M, outline, depth, color, slot=0, back_color=None):
        """Extrudes an outline of (x, z) points (counter-clockwise seen from the front, -Y) by depth along Y.
        The faces are fanned from the outline's centre, so it only has to be star-shaped, not convex."""
        cx = sum(p[0] for p in outline) / len(outline)
        cz = sum(p[1] for p in outline) / len(outline)
        n = len(outline)
        sides = []
        for y, col in ((-depth / 2, color), (depth / 2, back_color or color)):
            ring = [self.vert(M @ Vector((x, y, z)), col(Vector((x, y, z)), Vector((0, math.copysign(1, y), 0)))) for x, z in outline]
            centre = self.vert(M @ Vector((cx, y, cz)), col(Vector((cx, y, cz)), Vector((0, math.copysign(1, y), 0))))
            sides.append((ring, centre))
        (front, cf), (back, cb) = sides
        for i in range(n):
            j = (i + 1) % n
            self.face((cf, front[i], front[j]), slot)
            self.face((cb, back[j], back[i]), slot)
            self.face((front[i], back[i], back[j], front[j]), slot)

    def tube(self, path, radius, color, sides=7, sway=lambda t: 0.0, cap_start=True, cap_end=True, slot=0, wobble=0.0):
        """A tube along a list of points. radius(t) and color(t, angle) give size and colour; sway(t) the weight."""
        rings, n = [], len(path)
        normal = Vector((0, 0, 1)) if abs((path[1] - path[0]).normalized().z) < .9 else Vector((1, 0, 0))
        for i, p in enumerate(path):
            t = i / (n - 1)
            d = (path[min(i + 1, n - 1)] - path[max(i - 1, 0)]).normalized()
            side = d.cross(normal).normalized()
            normal = side.cross(d).normalized()
            ring = []
            for k in range(sides):
                a = 2 * math.pi * k / sides
                r = radius(t) * (1 + wobble * noise.noise(p * 3.1 + Vector((k, 0, 0))))
                ring.append(self.vert(p + (side * math.cos(a) + normal * math.sin(a)) * r, color(t, a), sway(t)))
            rings.append(ring)
        for i in range(n - 1):
            for k in range(sides):
                self.face((rings[i][k], rings[i][(k + 1) % sides], rings[i + 1][(k + 1) % sides], rings[i + 1][k]), slot)
        if cap_start:
            self.face(tuple(reversed(rings[0])), slot)
        if cap_end:
            self.face(tuple(rings[-1]), slot)
        return rings

    def lump(self, center, size, color_fn, subdiv=2, rough=.28, seed=0, flatten=None, slot=0, sway=0.0, M=None):
        """A displaced icosphere (rocks, mounds, leaves, blossoms). color_fn(position, normal) -> rgb."""
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
        base = len(self.v)
        off = Vector((seed * 7.3, seed * 1.7, seed * 3.1))
        for v in bm.verts:
            p = v.co.copy()
            d = 1 + rough * noise.noise(p * 1.6 + off) + rough * .45 * noise.noise(p * 3.7 + off)
            p = Vector((p.x * size[0], p.y * size[1], p.z * size[2])) * d
            if flatten is not None and p.z < flatten:
                p.z = flatten + (p.z - flatten) * .15
            v.co = p
        bm.normal_update()
        for v in bm.verts:
            q = Vector(center) + v.co
            self.vert(M @ q if M else q, color_fn(v.co, v.normal), sway)
        for f in bm.faces:
            self.face([base + v.index for v in f.verts], slot)
        bm.free()

    def build(self, materials):
        mesh = bpy.data.meshes.new(self.name)
        mesh.from_pydata([tuple(v) for v in self.v], [], self.f)
        mesh.validate()
        attr = mesh.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
        for i, c in enumerate(self.c):
            attr.data[i].color = c
        for poly, slot in zip(mesh.polygons, self.m):
            poly.material_index = slot
            poly.use_smooth = False
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        for mat in materials:
            mesh.materials.append(mat)
        return obj


def export(objects, out):
    """One FBX per piece, in Unreal's axes (front -Y, Z up), vertex colours in linear space."""
    os.makedirs(out, exist_ok=True)
    for obj in objects:
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(filepath=os.path.join(out, obj.name + ".fbx"), use_selection=True, object_types={"MESH"},
                                 axis_forward="-Y", axis_up="Z", colors_type="LINEAR", mesh_smooth_type="FACE", bake_anim=False)
        print(f"KIT {obj.name}: {len(obj.data.vertices)} verts, {len(obj.data.polygons)} faces")


def preview(objects, path, columns=8):
    """Lays the pieces out in a grid and renders them with their vertex colours (Workbench)."""
    x = y = row_depth = 0.0
    for i, obj in enumerate(objects):
        if i and i % columns == 0:
            x, y, row_depth = 0.0, y + row_depth + 1.5, 0.0
        size = max(obj.dimensions.x, 1.0)
        obj.location = (x + size / 2, y, 0)
        x += size + .8
        row_depth = max(row_depth, obj.dimensions.y, 2.0)
    width = max(o.location.x + o.dimensions.x / 2 for o in objects)
    depth = y + row_depth
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "VERTEX"
    scene.display.shading.show_shadows = True
    # The kit's colours are dark (moonlit); brighten the preview and use a grey backdrop so they can be judged.
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.exposure = 1.6
    scene.world = bpy.data.worlds.new("Backdrop")
    scene.world.color = (.35, .36, .4)
    scene.render.resolution_x, scene.render.resolution_y = 2400, 1500
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = max(width, depth * 1.6) * 1.08
    cam.location = (width / 2, depth / 2 - 60 * math.cos(math.radians(55)), 60 * math.sin(math.radians(55)))
    cam.rotation_euler = (math.radians(35), 0, 0)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
