# Blender: generates Hellgirl's stylised dark-forest kit (low-poly, faceted, colours baked into vertex colours)
# and exports one FBX per piece. Vertex colour alpha is the sway weight (0 = rigid, 1 = moves most) used by
# M_ForestKit. Material slot 0 is the lit kit material, slot 1 the glow material.
# Run through build_forest_kit.ps1, or:
#   blender -b --factory-startup -P forest_kit.py -- <output folder> [preview.png]
import bpy, math, os, random, sys
from mathutils import Matrix, Vector, noise

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = args[1] if len(args) > 1 else None
os.makedirs(OUT, exist_ok=True)


def mix(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def shade(c, f):
    return (c[0] * f, c[1] * f, c[2] * f)


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

    # A tube along a path. radius(t) and color(t, around) give size and colour; sway(t) the sway weight.
    def tube(self, path, radius, color, sides=7, sway=lambda t: 0.0, cap_start=True, cap_end=True, slot=0, wobble=0.0):
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
                q = p + (side * math.cos(a) + normal * math.sin(a)) * r
                ring.append(self.vert(q, color(t, a), sway(t)))
            rings.append(ring)
        for i in range(n - 1):
            for k in range(sides):
                a, b = rings[i][k], rings[i][(k + 1) % sides]
                c, d = rings[i + 1][(k + 1) % sides], rings[i + 1][k]
                self.face((a, b, c, d), slot)
        if cap_start:
            self.face(tuple(reversed(rings[0])), slot)
        if cap_end:
            self.face(tuple(rings[-1]), slot)
        return rings

    def lump(self, center, size, color_fn, subdiv=2, rough=.28, seed=0, flatten=None, slot=0, sway=0.0):
        """Displaced icosphere (rocks, moss, mounds, berries). color_fn(position, normal) -> rgb."""
        import bmesh
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
            self.vert(center + v.co, color_fn(v.co, v.normal), sway)
        for f in bm.faces:
            self.face([base + v.index for v in f.verts], slot)
        bm.free()

    def build(self):
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
        for mat in (KIT, GLOW):
            mesh.materials.append(mat)
        return obj


# Palette: dark, desaturated forest with hellish accents.
BARK, BARK_DARK, WOOD = (.3, .19, .11), (.16, .1, .065), (.62, .45, .27)
NEEDLE_DARK, NEEDLE, NEEDLE_TIP = (.03, .08, .05), (.06, .14, .08), (.13, .24, .12)
MOSS, MOSS_LIGHT = (.08, .16, .05), (.17, .28, .08)
STONE, STONE_DARK, OBSIDIAN = (.24, .24, .22), (.13, .13, .13), (.05, .035, .045)
DIRT, DIRT_DARK, BONE = (.22, .15, .09), (.05, .035, .025), (.72, .66, .53)
GRASS_BASE, GRASS_TIP, GRASS_DRY = (.05, .1, .04), (.16, .28, .09), (.3, .27, .12)
THORN, THORN_LEAF, EMBER = (.09, .03, .04), (.22, .04, .06), (1., .2, .03)
KIT = bpy.data.materials.new("Kit")
GLOW = bpy.data.materials.new("Glow")


def stone_color(p, n, moss=.55):
    c = mix(STONE_DARK, STONE, .5 + .5 * noise.noise(p * 2.3))
    return mix(c, MOSS, (n.z - moss) * 3.0 + .3 * noise.noise(p * 4.0))


# ---------------------------------------------------------------------------------------------------------
def pine(name, seed, height, tiers, spread, slim=1.0):
    P = Piece(name, seed)
    rng = P.rng
    bend = Vector((rng.uniform(-.3, .3), rng.uniform(-.3, .3), 0))
    trunk = [Vector((0, 0, 0)) + bend * (t * t) + Vector((0, 0, height * t)) for t in [i / 7 for i in range(8)]]
    P.tube(trunk, lambda t: .28 * (1 - t * .85) * slim + .03, lambda t, a: mix(BARK, BARK_DARK, .5 + .5 * math.sin(a * 3)),
           sides=7, wobble=.08, cap_start=False)
    base_z = height * .2
    for i in range(tiers):
        t = i / max(1, tiers - 1)
        z0 = base_z + (height * .92 - base_z) * t
        r = spread * (1 - t * .78) * slim + .25
        top = Vector((0, 0, z0 + height * .22 * (1 - t * .4))) + bend * ((z0 / height) ** 2)
        center = Vector((0, 0, z0)) + bend * ((z0 / height) ** 2)
        n = 9 + (tiers - i)
        apex = P.vert(top, NEEDLE_DARK, (z0 / height) * .5)
        outer, inner = [], []
        for k in range(n * 2):
            a = 2 * math.pi * k / (n * 2) + rng.uniform(-.08, .08)
            tip = k % 2 == 0
            rr = r * (1.0 if tip else .72) * rng.uniform(.9, 1.1)
            droop = (-.55 if tip else -.25) * r * .5
            p = center + Vector((math.cos(a) * rr, math.sin(a) * rr, droop))
            col = mix(NEEDLE, NEEDLE_TIP, .7 if tip else .2)
            col = shade(col, rng.uniform(.85, 1.1))
            outer.append(P.vert(p, col, min(1.0, z0 / height * .6 + (.5 if tip else .25))))
        for k in range(n * 2):
            P.face((apex, outer[k], outer[(k + 1) % (n * 2)]))
        # Underside, pulled up toward the trunk, closes the skirt so it reads solid from below.
        under = P.vert(center + Vector((0, 0, r * .12)), NEEDLE_DARK, (z0 / height) * .5)
        for k in range(n * 2):
            P.face((under, outer[(k + 1) % (n * 2)], outer[k]))
    return P


def dead_tree(name, seed, height):
    P = Piece(name, seed)
    rng = P.rng
    lean = Vector((rng.uniform(-.5, .5), rng.uniform(-.5, .5), 0))
    trunk = [Vector((0, 0, height * t)) + lean * t * t + Vector((.15 * math.sin(t * 5), .12 * math.cos(t * 4), 0)) for t in [i / 8 for i in range(9)]]
    grey = (.12, .1, .09)
    P.tube(trunk, lambda t: .32 * (1 - t * .8) + .02, lambda t, a: mix(grey, BARK_DARK, .5 + .5 * math.sin(a * 2 + t * 6)), sides=8, wobble=.12, cap_start=False)
    for b in range(rng.randint(4, 6)):
        t0 = rng.uniform(.35, .85)
        start = trunk[int(t0 * 8)]
        a = rng.uniform(0, 2 * math.pi)
        d = Vector((math.cos(a), math.sin(a), rng.uniform(.4, 1.1))).normalized()
        L = height * rng.uniform(.22, .38) * (1.1 - t0 * .5)
        path, p = [], start.copy()
        for s in range(5):
            path.append(p.copy())
            d = (d + Vector((rng.uniform(-.35, .35), rng.uniform(-.35, .35), rng.uniform(-.1, .25)))).normalized()
            p = p + d * (L / 4)
        P.tube(path, lambda t: .12 * (1 - t * .85) * (1.2 - t0 * .5), lambda t, a: grey, sides=5, wobble=.1, sway=lambda t: t * .4)
    return P


def rock(name, seed, size, moss=.55):
    P = Piece(name, seed)
    P.lump(Vector((0, 0, 0)), size, lambda p, n: stone_color(p, n, moss), subdiv=2, rough=.3, seed=seed, flatten=-size[2] * .35)
    return P


def standing_stone(name, seed, height, runes=True):
    P = Piece(name, seed)
    P.lump(Vector((0, 0, height * .45)), (.45, .32, height * .52), lambda p, n: stone_color(p, n, .75), subdiv=2, rough=.18, seed=seed, flatten=-height * .44)
    if runes:  # glowing rune marks on the front face
        rng = P.rng
        for i in range(4):
            z = height * (.35 + i * .13)
            x = rng.uniform(-.1, .1)
            w, h = rng.uniform(.06, .12), rng.uniform(.1, .16)
            y = -.36
            q = [P.vert((x - w, y, z - h), EMBER), P.vert((x + w, y, z - h * .3), EMBER), P.vert((x + w * .4, y, z + h), EMBER), P.vert((x - w * .7, y, z + h * .5), EMBER)]
            P.face(q, 1)
    return P


def log(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    L, R = 5.0, .45
    path = [Vector((-L / 2 + L * i / 8, 0, 0)) for i in range(9)]
    rings = P.tube(path, lambda t: R * (1 - .08 * t), lambda t, a: mix(BARK_DARK, BARK, .5 + .5 * math.sin(a * 5 + t * 3)), sides=12, wobble=.06, cap_start=False, cap_end=False)
    # Moss along the top: tint the upper vertices of every ring.
    for ring in rings:
        for i in ring:
            if P.v[i].z > R * .55:
                P.c[i] = mix(P.c[i][:3], MOSS_LIGHT, .8 + .2 * noise.noise(P.v[i] * 3)) + (0,)
    # Cut ends with growth rings.
    for end, sign in ((path[0], -1), (path[-1], 1)):
        center = P.vert(end + Vector((sign * .01, 0, 0)), shade(WOOD, .8))
        prev = [center]
        for ri, frac in enumerate((.35, .7, .95)):
            ring = []
            for k in range(12):
                a = 2 * math.pi * k / 12
                ring.append(P.vert(end + Vector((sign * .01, math.cos(a) * R * frac, math.sin(a) * R * frac)), shade(WOOD, 1.0 if ri % 2 else .75)))
            if len(prev) == 1:
                for k in range(12):
                    P.face((prev[0], ring[k], ring[(k + 1) % 12]) if sign > 0 else (prev[0], ring[(k + 1) % 12], ring[k]))
            else:
                for k in range(12):
                    q = (prev[k], ring[k], ring[(k + 1) % 12], prev[(k + 1) % 12])
                    P.face(q if sign > 0 else tuple(reversed(q)))
            prev = ring
    for s in range(3):
        x = rng.uniform(-1.8, 1.8)
        a = rng.uniform(.3, 2.8) * (1 if s % 2 else -1)
        start = Vector((x, math.cos(a) * R * .9, math.sin(a) * R * .9))
        d = Vector((rng.uniform(-.4, .4), math.cos(a), math.sin(a) + .3)).normalized()
        P.tube([start, start + d * .35, start + d * .6], lambda t: .09 * (1 - t * .6), lambda t, a2: BARK, sides=5)
    return P


def stump(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    H, R = 1.0, .7
    lobes = 5
    path = [Vector((0, 0, H * i / 5)) for i in range(6)]
    P.tube(path, lambda t: R * (1.35 - .35 * min(1, t * 3)), lambda t, a: mix(BARK_DARK, BARK, .5 + .5 * math.sin(a * 6)), sides=14, wobble=.1, cap_start=False, cap_end=False)
    top = H
    center = P.vert((0, 0, top), shade(WOOD, .7))
    prev = [center]
    for ri, frac in enumerate((.3, .6, .85, 1.0)):
        ring = [P.vert((math.cos(2 * math.pi * k / 14) * R * frac, math.sin(2 * math.pi * k / 14) * R * frac, top + .02 * (1 - frac)), shade(WOOD, .95 if ri % 2 else .7)) for k in range(14)]
        for k in range(14):
            if len(prev) == 1:
                P.face((prev[0], ring[k], ring[(k + 1) % 14]))
            else:
                P.face((prev[k], ring[k], ring[(k + 1) % 14], prev[(k + 1) % 14]))
        prev = ring
    for k in range(lobes):
        a = 2 * math.pi * k / lobes + rng.uniform(-.2, .2)
        d = Vector((math.cos(a), math.sin(a), 0))
        path = [d * R * .8 + Vector((0, 0, .45)), d * R * 1.4 + Vector((0, 0, .12)), d * R * 2.0 + Vector((0, 0, -.05)), d * R * 2.4 + Vector((0, 0, -.2))]
        P.tube(path, lambda t: .2 * (1 - t * .75), lambda t, a2: mix(BARK, MOSS, t * .6), sides=6, wobble=.1)
    return P


def fern(name, seed, fronds=8):
    P = Piece(name, seed)
    rng = P.rng
    for f in range(fronds):
        a = 2 * math.pi * f / fronds + rng.uniform(-.2, .2)
        d = Vector((math.cos(a), math.sin(a), 0))
        side = Vector((-d.y, d.x, 0))
        L = rng.uniform(.8, 1.2)
        rise = rng.uniform(.5, .8)
        left, right, spine = [], [], []
        steps = 8
        for s in range(steps + 1):
            t = s / steps
            p = d * L * t + Vector((0, 0, rise * math.sin(t * math.pi * .85) - t * t * .15))
            w = .16 * math.sin(math.pi * min(1, t * 1.05)) * (1.3 if s % 2 else 1.0)
            col = mix(GRASS_BASE, MOSS_LIGHT, t * .9)
            spine.append(P.vert(p, col, t))
            left.append(P.vert(p + side * w + Vector((0, 0, -.03)), shade(col, .9), t))
            right.append(P.vert(p - side * w + Vector((0, 0, -.03)), shade(col, 1.05), t))
        for s in range(steps):
            P.face((spine[s], left[s], left[s + 1], spine[s + 1]))
            P.face((spine[s], spine[s + 1], right[s + 1], right[s]))
    return P


def grass(name, seed, blades=14, dry=.15):
    P = Piece(name, seed)
    rng = P.rng
    for b in range(blades):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0, .18)
        base = Vector((math.cos(a) * r, math.sin(a) * r, 0))
        lean = Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(.1, .35)
        H = rng.uniform(.35, .7)
        w = rng.uniform(.03, .05)
        side = Vector((-math.sin(a + 1.2), math.cos(a + 1.2), 0))
        tip = mix(GRASS_TIP, GRASS_DRY, 1.0 if rng.random() < dry else 0.0)
        pts = []
        for s in range(4):
            t = s / 3
            c = base + lean * t * t + Vector((0, 0, H * t))
            col = mix(GRASS_BASE, tip, t)
            if s < 3:
                pts.append((P.vert(c + side * w * (1 - t), col, t), P.vert(c - side * w * (1 - t), col, t)))
            else:
                pts.append((P.vert(c, col, 1.0),))
        for s in range(2):
            (a1, b1), (a2, b2) = pts[s], pts[s + 1]
            P.face((a1, b1, b2, a2))
        (a1, b1), (tipv,) = pts[2], pts[3]
        P.face((a1, b1, tipv))
    return P


def mushrooms(name, seed, glow):
    P = Piece(name, seed)
    rng = P.rng
    stem_c = (.62, .56, .45)
    cap_c = (.35, .08, .05)
    for m in range(rng.randint(3, 5)):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0, .18)
        base = Vector((math.cos(a) * r, math.sin(a) * r, 0))
        H = rng.uniform(.1, .28)
        R = rng.uniform(.06, .13)
        tilt = Vector((rng.uniform(-.05, .05), rng.uniform(-.05, .05), 0))
        top = base + tilt + Vector((0, 0, H))
        P.tube([base, base + tilt * .5 + Vector((0, 0, H * .5)), top], lambda t: R * .32, lambda t, a2: stem_c, sides=6, cap_start=False)
        # Cap: a flattened dome with a rim.
        n = 9
        rim = [P.vert(top + Vector((math.cos(2 * math.pi * k / n) * R, math.sin(2 * math.pi * k / n) * R, -R * .15)), cap_c if not glow else (.2, .9, .75)) for k in range(n)]
        mid = [P.vert(top + Vector((math.cos(2 * math.pi * k / n) * R * .7, math.sin(2 * math.pi * k / n) * R * .7, R * .3)), cap_c if not glow else (.25, 1, .85)) for k in range(n)]
        apex = P.vert(top + Vector((0, 0, R * .5)), cap_c if not glow else (.4, 1, .9))
        slot = 1 if glow else 0
        for k in range(n):
            P.face((rim[k], rim[(k + 1) % n], mid[(k + 1) % n], mid[k]), slot)
            P.face((mid[k], mid[(k + 1) % n], apex), slot)
        under = P.vert(top + Vector((0, 0, -R * .05)), shade(stem_c, .6))
        for k in range(n):
            P.face((rim[(k + 1) % n], rim[k], under))
    return P


def bramble(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    for v in range(11):
        a = rng.uniform(0, 2 * math.pi)
        start = Vector((math.cos(a) * rng.uniform(0, .4), math.sin(a) * rng.uniform(0, .4), 0))
        d = Vector((math.cos(a + rng.uniform(-1, 1)), math.sin(a + rng.uniform(-1, 1)), rng.uniform(.6, 1.2))).normalized()
        path, p = [], start
        for s in range(7):
            path.append(p.copy())
            d = (d + Vector((rng.uniform(-.5, .5), rng.uniform(-.5, .5), -.28))).normalized()
            p = p + d * .2
        P.tube(path, lambda t: .03 * (1 - t * .5), lambda t, a2: mix(THORN, (.16, .05, .06), t), sides=5, sway=lambda t: t * .3)
        for s in range(1, 6):
            q = path[s]
            for k in range(2):
                out = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-.2, 1))).normalized()
                b = [P.vert(q + Vector((.012, 0, 0)), THORN), P.vert(q + Vector((-.012, 0, .01)), THORN), P.vert(q + Vector((0, .012, -.01)), THORN)]
                tip = P.vert(q + out * .09, (.35, .3, .25))
                P.face((b[0], b[1], tip)); P.face((b[1], b[2], tip)); P.face((b[2], b[0], tip))
            if rng.random() < .5:  # a dark red leaf
                side = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(0, .5))).normalized()
                l = [P.vert(q, THORN_LEAF, .5), P.vert(q + side * .07 + Vector((0, 0, .04)), THORN_LEAF, .6),
                     P.vert(q + side * .15, shade(THORN_LEAF, 1.3), .7), P.vert(q + side * .07 - Vector((0, 0, .04)), THORN_LEAF, .6)]
                P.face(l)
        for k in range(rng.randint(2, 4)):  # glowing berries
            q = path[rng.randint(2, 6)] + Vector((0, 0, .02))
            P.lump(q, (.035, .035, .035), lambda p2, n2: EMBER, subdiv=1, rough=.05, seed=seed + v * 10 + k, slot=1)
    return P


def burrow(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    # Lumpy dirt ring around a dark hole.
    n = 16
    for k in range(n):
        a = 2 * math.pi * k / n
        P.lump(Vector((math.cos(a) * 1.05, math.sin(a) * 1.05, 0)), (.42, .42, .26 + .12 * rng.random()),
               lambda p, nn: mix(DIRT_DARK, DIRT, .6 + .4 * nn.z), subdiv=1, rough=.25, seed=seed + k, flatten=-.05)
    hole = [P.vert((math.cos(2 * math.pi * k / 12) * .85, math.sin(2 * math.pi * k / 12) * .85, .03), (.01, .007, .005)) for k in range(12)]
    center = P.vert((0, 0, -.2), (0, 0, 0))
    for k in range(12):
        P.face((center, hole[(k + 1) % 12], hole[k]))
    # Bones and crude stakes.
    for b in range(4):
        a = rng.uniform(0, 2 * math.pi)
        p = Vector((math.cos(a) * 1.6, math.sin(a) * 1.6, .05))
        d = Vector((math.cos(a + 1.6), math.sin(a + 1.6), 0))
        P.tube([p - d * .22, p + d * .22], lambda t: .035 + .03 * abs(t - .5) * 2, lambda t, a2: BONE, sides=5)
    for s in range(3):
        a = rng.uniform(0, 2 * math.pi)
        p = Vector((math.cos(a) * 1.45, math.sin(a) * 1.45, 0))
        lean = Vector((math.cos(a), math.sin(a), 0)) * .25
        P.tube([p, p + lean + Vector((0, 0, 1.1))], lambda t: .06 * (1 - t * .7), lambda t, a2: BARK, sides=5, cap_end=False)
        tip = P.vert(p + lean * 1.1 + Vector((0, 0, 1.3)), BARK)
    return P


def hell_rift(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    # Obsidian shards leaning inward around a burning crack.
    for k in range(9):
        a = 2 * math.pi * k / 9 + rng.uniform(-.12, .12)
        h = rng.uniform(.8, 1.8)
        base = Vector((math.cos(a) * 1.35, math.sin(a) * 1.35, 0))
        inward = -Vector((math.cos(a), math.sin(a), 0)) * .35
        w = rng.uniform(.16, .28)
        side = Vector((-math.sin(a), math.cos(a), 0))
        out = Vector((math.cos(a), math.sin(a), 0))
        bottom = [base + side * w + out * .1, base - side * w + out * .1, base - side * w * .6 - out * .15, base + side * w * .6 - out * .15]
        tip = base + inward + Vector((0, 0, h))
        ids = [P.vert(q, OBSIDIAN) for q in bottom]
        t = P.vert(tip, (.12, .03, .03))
        for i in range(4):
            P.face((ids[i], ids[(i + 1) % 4], t))
        # a glowing seam up the inside of each shard
        seam = [P.vert(base - out * .16 + side * .03 + Vector((0, 0, .1)), EMBER), P.vert(base - out * .16 - side * .03 + Vector((0, 0, .1)), EMBER),
                P.vert(base + inward * .8 - out * .05 + Vector((0, 0, h * .8)), EMBER)]
        P.face(seam, 1)
    # The cracked, glowing floor.
    P.lump(Vector((0, 0, -.02)), (1.25, 1.25, .06), lambda p, n: (.02, .01, .01), subdiv=2, rough=.15, seed=seed)
    for c in range(7):
        a = 2 * math.pi * c / 7 + rng.uniform(-.3, .3)
        pts = [Vector((0, 0, .06))]
        for s in range(3):
            pts.append(pts[-1] + Vector((math.cos(a + rng.uniform(-.5, .5)), math.sin(a + rng.uniform(-.5, .5)), 0)) * .38)
        for i in range(len(pts) - 1):
            d = (pts[i + 1] - pts[i]).normalized()
            side = Vector((-d.y, d.x, 0)) * (.06 * (1 - i / 3) + .015)
            q = [P.vert(pts[i] + side + Vector((0, 0, .05)), EMBER), P.vert(pts[i + 1] + side * .6 + Vector((0, 0, .05)), EMBER),
                 P.vert(pts[i + 1] - side * .6 + Vector((0, 0, .05)), EMBER), P.vert(pts[i] - side + Vector((0, 0, .05)), EMBER)]
            P.face(q, 1)
    return P


def gateway(name, seed):
    """Two rune stones and a lintel: the forest room's exit."""
    P = Piece(name, seed)
    for side in (-1, 1):
        P.lump(Vector((0, side * 1.45, 1.7)), (.45, .38, 1.9), lambda p, n: stone_color(p, n, .8), subdiv=2, rough=.14, seed=seed + side, flatten=-1.75)
        for i in range(5):
            z = .9 + i * .45
            q = [P.vert((-.43, side * 1.45 - .1, z - .12), (.3, 1, .35)), P.vert((-.43, side * 1.45 + .1, z - .05), (.3, 1, .35)),
                 P.vert((-.43, side * 1.45 + .05, z + .12), (.3, 1, .35)), P.vert((-.43, side * 1.45 - .12, z + .06), (.3, 1, .35))]
            P.face(q, 1)
    P.lump(Vector((0, 0, 3.75)), (.5, 2.2, .38), lambda p, n: stone_color(p, n, .5), subdiv=2, rough=.12, seed=seed + 5)
    return P


def campfire(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    for k in range(11):
        a = 2 * math.pi * k / 11
        P.lump(Vector((math.cos(a) * .95, math.sin(a) * .95, .12)), (.24, .2, .17), lambda p, n: stone_color(p, n, .9), subdiv=1, rough=.25, seed=seed + k, flatten=-.1)
    for k in range(6):
        a = 2 * math.pi * k / 6 + rng.uniform(-.15, .15)
        foot = Vector((math.cos(a) * .6, math.sin(a) * .6, .05))
        top = Vector((math.cos(a) * .08, math.sin(a) * .08, .75))
        P.tube([foot, (foot + top) / 2, top], lambda t: .07, lambda t, a2: mix((.05, .03, .02), (.02, .01, .01), t), sides=6)
    P.lump(Vector((0, 0, .05)), (.5, .5, .06), lambda p, n: (.04, .03, .03), subdiv=1, rough=.2, seed=seed)
    for e in range(8):  # glowing embers in the ash
        a, r = rng.uniform(0, 2 * math.pi), rng.uniform(0, .4)
        P.lump(Vector((math.cos(a) * r, math.sin(a) * r, .1)), (.06, .05, .03), lambda p, n: EMBER, subdiv=1, rough=.2, seed=seed + 40 + e, slot=1)
    return P


pieces = [
    pine("SM_PineA", 1, 9.5, 6, 2.6), pine("SM_PineB", 2, 11.5, 7, 2.0, slim=.8), pine("SM_PineC", 3, 7.0, 5, 2.3),
    dead_tree("SM_DeadTree", 4, 8.0),
    rock("SM_RockA", 5, (1.4, 1.1, .9)), rock("SM_RockB", 6, (1.0, 1.2, 1.1)), rock("SM_RockC", 7, (1.6, .9, .55), moss=.35),
    standing_stone("SM_StandingStone", 8, 3.2),
    log("SM_Log", 9), stump("SM_Stump", 10),
    fern("SM_Fern", 11), grass("SM_GrassTuft", 12), grass("SM_GrassTuftDry", 13, blades=10, dry=.8),
    mushrooms("SM_Mushrooms", 14, False), mushrooms("SM_GlowShrooms", 15, True),
    bramble("SM_Bramble", 16), burrow("SM_Burrow", 17), hell_rift("SM_HellRift", 18), gateway("SM_Gateway", 19),
    campfire("SM_Campfire", 20),
]
bpy.ops.wm.read_factory_settings(use_empty=True)
KIT = bpy.data.materials.new("Kit")
GLOW = bpy.data.materials.new("Glow")
objects = [p.build() for p in pieces]
for obj in objects:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, obj.name + ".fbx"), use_selection=True, object_types={"MESH"},
                             axis_forward="-Y", axis_up="Z", colors_type="LINEAR", mesh_smooth_type="FACE", bake_anim=False)
    print(f"KIT {obj.name}: {len(obj.data.vertices)} verts, {len(obj.data.polygons)} faces")

if PREVIEW:
    # Line the pieces up and render them with their vertex colours.
    x = 0.0
    for obj in objects:
        size = max(obj.dimensions.x, obj.dimensions.y, 1.0)
        obj.location = (x + size / 2, 0, 0)
        x += size + .6
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "VERTEX"
    scene.display.shading.show_shadows = True
    scene.render.resolution_x, scene.render.resolution_y = 2400, 700
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = x * 1.02
    cam.location = (x / 2, -60, 12)
    cam.rotation_euler = (math.radians(82), 0, 0)
    scene.render.filepath = PREVIEW
    bpy.ops.render.render(write_still=True)
print("FOREST KIT DONE")
