# Blender: generates the Frozen Maze kit (World IV, the Deprived's icy cave): jagged ice walls built from crystal
# columns, the pillars where walls meet, ice spires, glowing crystal clusters, roof icicles, the ice bridge and the big
# icicle of the icicle traps, frozen remains (a figure in a block of ice), the Deprived's shadow pool and ice rubble.
# Low-poly with vertex colours like the other kits; slot 1 is the glowing (or, for the remains, the clear ice) part.
# Run through build_maze_kit.ps1, or:
#   blender -b --factory-startup -P maze_kit.py -- <output folder> [preview.png]
import bpy, math, os, sys
from mathutils import Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from kit_common import Piece, mix, shade, place, export, preview

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = args[1] if len(args) > 1 else None

ICE_DEEP, ICE_MID, ICE_LIGHT, FROST = (.025, .06, .12), (.07, .17, .3), (.26, .45, .64), (.55, .72, .86)
CRYSTAL, CRYSTAL_BRIGHT = (.2, .72, 1.0), (.6, .95, 1.0)
SHADOW, SHADOW_EDGE, VIOLET = (.003, .003, .005), (.02, .015, .03), (.5, .1, 1.0)
FIGURE, FIGURE_DARK = (.05, .055, .07), (.02, .022, .03)

WALL_LENGTH = 8.0     # one grid square of the maze
WALL_HALF = .62       # half the wall's thickness (the corridors keep about 6.6 m)


def frame(axis):
    """Two unit vectors square to axis (the same convention as kit_common's tubes)."""
    normal = Vector((0, 0, 1)) if abs(axis.z) < .9 else Vector((1, 0, 0))
    side = axis.cross(normal).normalized()
    return side, side.cross(axis).normalized()


def crystal(P, base, axis, length, radius, color, sides=6, tip=.3, taper=.82, slot=0, stretch=1.0):
    """A faceted crystal: a prism from base along axis, narrowing a little, ending in a point (tip = share of length).
    color(t, angle) with t from 0 (base) to 1 (point). stretch widens the cross-section (along X for an upright one)."""
    axis = axis.normalized()
    side, normal = frame(axis)
    turn = P.rng.uniform(0, 2 * math.pi) if stretch == 1.0 else P.rng.uniform(-.2, .2)
    rings = []
    shoulder = length * (1 - tip)
    for t, r in ((0.0, radius), (shoulder * .55, radius * (1 + taper) / 2 * 1.04), (shoulder, radius * taper)):
        ring = []
        for k in range(sides):
            a = turn + 2 * math.pi * k / sides
            ring.append(P.vert(base + axis * t + (side * math.cos(a) + normal * math.sin(a) * stretch) * r, color(t / length, a)))
        rings.append(ring)
    apex = P.vert(base + axis * length + side * P.rng.uniform(-.1, .1) * radius, color(1.0, 0.0))
    for i in range(2):
        for k in range(sides):
            j = (k + 1) % sides
            P.face((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]), slot)
    for k in range(sides):
        P.face((rings[2][k], rings[2][(k + 1) % sides], apex), slot)
    P.face(tuple(reversed(rings[0])), slot)


def ice(seed, dark=0.0):
    """Ice colour along a column: deep blue at the foot, frosted toward the point, facets lit unevenly."""
    def color(t, a):
        facet = .72 + .45 * abs(math.sin(a * 1.7 + seed * .37))
        c = mix(ICE_DEEP, ICE_LIGHT, t ** 1.2) if t < .92 else mix(ICE_LIGHT, FROST, (t - .92) / .08)
        return shade(c, facet * (1 - dark))
    return color


def lump_color(lo, hi):
    return lambda p, n: mix(lo, hi, .5 + .5 * n.z + .25 * noise.noise(p * 2.3))


def ice_wall(name, seed):
    """One 8 m stretch of maze wall along X, about 12 m tall: a solid core behind a row of crystal columns of uneven
    height, shards breaking the skyline, icicles under the shoulders and ice rubble at the foot on both sides."""
    P = Piece(name, seed)
    rng = P.rng
    L = WALL_LENGTH
    P.box(place(0, 0, 3.6), (L + .2, WALL_HALF * 1.2, 7.2), lambda p, n: mix(ICE_DEEP, ICE_MID, (p.z + 3.6) / 7.2), jitter=.04)
    # Wide, overlapping columns (stretched along the wall) so the faces are all crystal, never the flat core.
    count = 9
    for k in range(count):
        x = -L / 2 + (k + .5) * L / count + rng.uniform(-.15, .15)
        r = rng.uniform(.5, .6)
        y = rng.uniform(-.08, .08)
        height = rng.uniform(9.0, 12.4) + (1.8 if rng.random() < .2 else 0.0)
        axis = Vector((rng.uniform(-.04, .04), rng.uniform(-.03, .03), 1))
        crystal(P, Vector((x, y, -.3)), axis, height, r, ice(seed + k), sides=6, tip=rng.uniform(.08, .16), stretch=rng.uniform(1.25, 1.55))
    # Shards leaning out of the top, so the skyline is jagged rather than a fence.
    for k in range(rng.randint(3, 5)):
        x = rng.uniform(-L / 2 + .6, L / 2 - .6)
        side = rng.choice((-1, 1))
        axis = Vector((rng.uniform(-.4, .4), side * rng.uniform(.25, .6), 1))
        crystal(P, Vector((x, side * .2, rng.uniform(8.5, 10.5))), axis, rng.uniform(1.6, 3.4), rng.uniform(.18, .32), ice(seed + 20 + k), sides=5, tip=.35)
    # Icicles hanging from the shoulders on both faces.
    for k in range(rng.randint(5, 8)):
        x = rng.uniform(-L / 2 + .3, L / 2 - .3)
        side = rng.choice((-1, 1))
        crystal(P, Vector((x, side * (WALL_HALF + .05), rng.uniform(8.0, 10.0))), Vector((0, side * .08, -1)), rng.uniform(.6, 1.9), rng.uniform(.08, .16),
                lambda t, a: mix(ICE_LIGHT, FROST, t), sides=5, tip=.6)
    # Rubble at the foot.
    for k in range(rng.randint(4, 6)):
        x = rng.uniform(-L / 2 + .4, L / 2 - .4)
        side = rng.choice((-1, 1))
        P.lump(Vector((x, side * (WALL_HALF + .05), .1)), (rng.uniform(.35, .7), rng.uniform(.22, .32), rng.uniform(.25, .5)),
               lump_color(ICE_DEEP, ICE_MID), subdiv=1, rough=.3, seed=seed * 10 + k, flatten=0.0)
    return P


def ice_pillar(name, seed):
    """Where walls meet or end: a thick knot of taller columns that hides the joints."""
    P = Piece(name, seed)
    rng = P.rng
    for k in range(5):
        a = 2 * math.pi * k / 5 + rng.uniform(-.3, .3)
        d = rng.uniform(.15, .35) if k else 0.0
        base = Vector((math.cos(a) * d, math.sin(a) * d, -.3))
        axis = Vector((math.cos(a) * d * .15, math.sin(a) * d * .15, 1))
        crystal(P, base, axis, rng.uniform(11.5, 14.5) - k * .4, rng.uniform(.55, .72), ice(seed + k), sides=6, tip=rng.uniform(.1, .18))
    for k in range(4):
        a = rng.uniform(0, 2 * math.pi)
        P.lump(Vector((math.cos(a) * .7, math.sin(a) * .7, .1)), (.5, .45, .45), lump_color(ICE_DEEP, ICE_MID), subdiv=1, rough=.3, seed=seed + k, flatten=0.0)
    return P


def ice_spire(name, seed, count, tallest):
    """Free-standing crystal spires for the rooms: a tall centre with others leaning out around it."""
    P = Piece(name, seed)
    rng = P.rng
    crystal(P, Vector((0, 0, -.3)), Vector((rng.uniform(-.06, .06), rng.uniform(-.06, .06), 1)), tallest, tallest * .09, ice(seed), sides=6, tip=.22)
    for k in range(count):
        a = 2 * math.pi * k / count + rng.uniform(-.3, .3)
        d = Vector((math.cos(a), math.sin(a), 0))
        lean = rng.uniform(.12, .5)
        length = tallest * rng.uniform(.35, .75)
        crystal(P, d * rng.uniform(.3, .7) + Vector((0, 0, -.3)), d * lean + Vector((0, 0, 1)), length, length * rng.uniform(.08, .11), ice(seed + k + 1), sides=rng.choice((5, 6)), tip=.25)
    for k in range(5):
        a = rng.uniform(0, 2 * math.pi)
        P.lump(Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(.6, 1.2), (.55, .5, .4), lump_color(ICE_DEEP, ICE_MID), subdiv=1, rough=.3, seed=seed + 40 + k, flatten=0.0)
    return P


def glow_crystals(name, seed):
    """A cluster of glowing cyan crystals growing out of the foot of a wall (front = -Y, into the corridor)."""
    P = Piece(name, seed)
    rng = P.rng
    P.lump(Vector((0, .2, .1)), (1.0, .6, .45), lump_color(ICE_DEEP, ICE_MID), subdiv=1, rough=.3, seed=seed, flatten=0.0)
    glow = lambda t, a: mix(CRYSTAL, CRYSTAL_BRIGHT, t)
    for k in range(rng.randint(8, 11)):
        a = rng.uniform(-math.pi * .9, -math.pi * .1)  # fanned toward the front and up
        out = Vector((math.cos(a), math.sin(a), 0))
        axis = out * rng.uniform(.2, .9) + Vector((0, 0, 1))
        length = rng.uniform(.5, 2.4) if k else 2.8
        crystal(P, Vector((rng.uniform(-.5, .5), rng.uniform(0, .35), .1)), axis, length, length * rng.uniform(.1, .14), glow, sides=6, tip=.3, slot=1)
    return P


def roof_icicles(name, seed):
    """Icicles hanging from the cave roof: a frozen knot with long points below (pivot at the top)."""
    P = Piece(name, seed)
    rng = P.rng
    P.lump(Vector((0, 0, -.2)), (1.6, 1.4, .6), lump_color(ICE_MID, ICE_LIGHT), subdiv=1, rough=.3, seed=seed)
    for k in range(rng.randint(8, 13)):
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0, 1.3)
        length = rng.uniform(1.0, 6.5) * (1 - d * .4)
        crystal(P, Vector((math.cos(a) * d, math.sin(a) * d, -.3)), Vector((rng.uniform(-.05, .05), rng.uniform(-.05, .05), -1)), length, length * .07 + .08,
                lambda t, a2: mix(ICE_MID, FROST, t), sides=5, tip=.55)
    return P


def ice_bridge(name, seed):
    """An ice beam across a corridor at the top of its walls (along X, 8.6 m, resting on both walls), hung with icicles."""
    P = Piece(name, seed)
    rng = P.rng
    P.box(place(0, 0, 0), (8.6, 1.0, 1.1), lambda p, n: mix(ICE_MID, ICE_LIGHT, .5 + p.z), jitter=.1)
    for k in range(6):
        P.lump(Vector((-3.8 + k * 1.52 + rng.uniform(-.2, .2), 0, .1)), (.9, .7, .6), lump_color(ICE_MID, FROST), subdiv=1, rough=.3, seed=seed + k)
    for k in range(rng.randint(9, 13)):
        x = rng.uniform(-3.8, 3.8)
        if abs(x) < .7:
            continue  # the trap's big icicle hangs in the middle
        crystal(P, Vector((x, rng.uniform(-.35, .35), -.45)), Vector((0, 0, -1)), rng.uniform(.4, 1.6), rng.uniform(.07, .14), lambda t, a: mix(ICE_LIGHT, FROST, t), sides=5, tip=.6)
    return P


def big_icicle(name, seed):
    """The trap's icicle: 3.4 m, hanging point down (pivot at the top)."""
    P = Piece(name, seed)
    rng = P.rng
    crystal(P, Vector((0, 0, 0)), Vector((0, 0, -1)), 3.4, .42, lambda t, a: mix(ICE_LIGHT, FROST, t), sides=6, tip=.5, taper=.75)
    for k in range(3):
        a = 2 * math.pi * k / 3 + rng.uniform(-.3, .3)
        crystal(P, Vector((math.cos(a) * .3, math.sin(a) * .3, -.1)), Vector((math.cos(a) * .15, math.sin(a) * .15, -1)), rng.uniform(1.0, 1.8), .15,
                lambda t, a2: mix(ICE_LIGHT, FROST, t), sides=5, tip=.55)
    return P


def frozen_remains(name, seed):
    """One of the Deprived's victims, frozen kneeling in a block of clear ice with an arm raised (slot 0 the figure,
    slot 1 the ice around it)."""
    P = Piece(name, seed)
    body = lambda t, a: mix(FIGURE, FIGURE_DARK, t)
    skin = lambda p, n: mix(FIGURE_DARK, FIGURE, .5 + .5 * n.z)
    P.lump(Vector((.02, -.05, 1.52)), (.12, .13, .15), skin, subdiv=1, rough=.1, seed=seed)
    P.tube([Vector((0, 0, .72)), Vector((.02, -.02, 1.05)), Vector((.02, -.04, 1.38))], lambda t: .17 - t * .03, body, sides=6)
    P.tube([Vector((.2, -.02, 1.33)), Vector((.3, -.12, 1.62)), Vector((.33, -.2, 1.95))], lambda t: .055, body, sides=5)      # reaching up
    P.tube([Vector((-.2, -.02, 1.33)), Vector((-.22, -.25, 1.2)), Vector((-.02, -.3, 1.45))], lambda t: .055, body, sides=5)  # across the face
    for sx in (-.12, .12):
        P.tube([Vector((sx, 0, .75)), Vector((sx * 1.3, -.34, .38)), Vector((sx * 1.3, .12, .08))], lambda t: .08 - t * .02, body, sides=5)
    P.lump(Vector((0, 0, 1.1)), (.82, .66, 1.22), lambda p, n: mix(ICE_MID, FROST, .5 + .5 * n.z), subdiv=1, rough=.12, seed=seed + 3, flatten=-.05, slot=1)
    return P


def shadow_pool(name, seed):
    """The Deprived's lair: a pool of black shadow on the ice, ringed by dark shards, a violet glow at its edge and
    violet veins running out from it."""
    P = Piece(name, seed)
    rng = P.rng
    n = 28
    radius = [2.0 + .35 * noise.noise(Vector((math.cos(2 * math.pi * k / n) * 1.3, math.sin(2 * math.pi * k / n) * 1.3, seed))) for k in range(n)]
    centre = P.vert(Vector((0, 0, .03)), SHADOW)
    ring = [P.vert(Vector((math.cos(2 * math.pi * k / n) * radius[k], math.sin(2 * math.pi * k / n) * radius[k], .03)), SHADOW_EDGE) for k in range(n)]
    for k in range(n):
        P.face((centre, ring[(k + 1) % n], ring[k]))
    # The glowing edge: a thin band just outside the pool.
    outer = [P.vert(Vector((math.cos(2 * math.pi * k / n) * (radius[k] + .12), math.sin(2 * math.pi * k / n) * (radius[k] + .12), .025)), VIOLET) for k in range(n)]
    for k in range(n):
        j = (k + 1) % n
        P.face((ring[k], ring[j], outer[j], outer[k]), 1)
    for k in range(rng.randint(6, 9)):
        a = rng.uniform(0, 2 * math.pi)
        d = Vector((math.cos(a), math.sin(a), 0))
        s = Vector((-d.y, d.x, 0)) * .04
        start = d * 2.0
        length = rng.uniform(.6, 1.6)
        pts = [start + d * length * t + s * 3 * math.sin(t * 5 + k) for t in (0, .33, .66, 1)]
        ids = [(P.vert(p + s * (1 - t) + Vector((0, 0, .02)), VIOLET), P.vert(p - s * (1 - t) + Vector((0, 0, .02)), VIOLET)) for p, t in zip(pts, (0, .33, .66, 1))]
        for i in range(3):
            P.face((ids[i][0], ids[i + 1][0], ids[i + 1][1], ids[i][1]), 1)
    for k in range(rng.randint(7, 10)):
        a = 2 * math.pi * k / 9 + rng.uniform(-.2, .2)
        d = Vector((math.cos(a), math.sin(a), 0))
        crystal(P, d * rng.uniform(2.1, 2.5), d * rng.uniform(.3, .8) + Vector((0, 0, 1)), rng.uniform(.4, 1.3), rng.uniform(.08, .16),
                lambda t, a2: mix(SHADOW_EDGE, (.08, .05, .14), t), sides=5, tip=.4)
    return P


def ice_rubble(name, seed):
    """Loose chunks of ice for the foot of walls and room corners."""
    P = Piece(name, seed)
    rng = P.rng
    for k in range(rng.randint(3, 5)):
        s = rng.uniform(.2, .6)
        P.lump(Vector((rng.uniform(-.9, .9), rng.uniform(-.5, .5), 0)), (s, s * rng.uniform(.7, 1.1), s * rng.uniform(.5, .9)),
               lump_color(ICE_DEEP, ICE_LIGHT), subdiv=1, rough=.35, seed=seed + k, flatten=0.0)
    for k in range(2):
        crystal(P, Vector((rng.uniform(-.6, .6), rng.uniform(-.3, .3), -.05)), Vector((rng.uniform(-.6, .6), rng.uniform(-.6, .6), 1)), rng.uniform(.5, 1.1), .12, ice(seed + k), sides=5, tip=.3)
    return P


pieces = [
    ice_wall("SM_IceWallA", 11), ice_wall("SM_IceWallB", 23), ice_wall("SM_IceWallC", 37),
    ice_pillar("SM_IcePillar", 5),
    ice_spire("SM_IceSpireA", 3, 7, 7.5), ice_spire("SM_IceSpireB", 9, 5, 4.5),
    glow_crystals("SM_GlowCrystals", 4),
    roof_icicles("SM_RoofIcicles", 8),
    ice_bridge("SM_IceBridge", 12), big_icicle("SM_BigIcicle", 2),
    frozen_remains("SM_FrozenRemains", 6),
    shadow_pool("SM_ShadowPool", 13),
    ice_rubble("SM_IceRubble", 17),
]
materials = [bpy.data.materials.new("Ice"), bpy.data.materials.new("IceGlow")]
objects = [p.build(materials) for p in pieces]
export(objects, OUT)
if PREVIEW:
    preview(objects, PREVIEW, columns=len(objects))
print("MAZE KIT DONE")
