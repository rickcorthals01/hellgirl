# Blender: generates the graveyard kit for World IV (the ghosts' graveyard): headstones, crosses, graves, mausoleums,
# the iron-railed wall, the gate and the crypt exit, roses, hedges, a dead oak, a well, statues, candles and ghost
# lanterns. Low-poly and faceted like the forest kit, colours in vertex colours (alpha = sway), slot 1 glows.
# Run through build_graveyard_kit.ps1, or:
#   blender -b --factory-startup -P graveyard_kit.py -- <output folder> [preview.png]
import bpy, math, os, sys
from mathutils import Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from kit_common import Piece, mix, shade, place, I, export, preview

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = args[1] if len(args) > 1 else None

# Palette: moonlit, blue-grey stone and near-black greens; the roses are the only warm colour.
STONE, STONE_LIGHT, STONE_DARK = (.34, .35, .37), (.5, .51, .53), (.16, .17, .19)
MOSS, LICHEN = (.05, .09, .06), (.24, .27, .2)
IRON, IRON_RUST = (.035, .035, .04), (.12, .06, .04)
DIRT, DIRT_DARK, PIT = (.1, .075, .055), (.045, .033, .026), (.012, .01, .01)
GRASS_BASE, GRASS_TIP = (.02, .045, .035), (.07, .13, .1)
LEAF, LEAF_DARK = (.03, .075, .045), (.012, .03, .02)
ROSE, ROSE_DARK = (.55, .02, .04), (.22, .006, .015)
WOOD, WOOD_DARK = (.2, .14, .09), (.08, .055, .04)
BARK = (.09, .085, .08)
BONE, WAX = (.62, .6, .52), (.68, .66, .58)
GHOST = (.55, .85, 1.0)   # ghost-light flames and the crypt's glow


def stone(p, n, moss=.5, base=STONE):
    """Weathered stone: patchy light and dark, moss creeping up from the ground and on the tops, lichen specks."""
    c = mix(STONE_DARK, base, .55 + .45 * noise.noise(p * 3.1))
    c = mix(c, LICHEN, max(0.0, noise.noise(p * 9.0 + Vector((3, 1, 2))) - .35) * 1.2)
    c = mix(c, MOSS, max(0.0, (moss - p.z) * 1.4) + max(0.0, n.z - .6) * .5 * moss)
    return c


def flat(color):
    return lambda p, n: color


def iron(p, n):
    return mix(IRON, IRON_RUST, max(0.0, noise.noise(p * 6.0)) * .8)


# --- Headstones ---------------------------------------------------------------------------------------------
def base_plinth(P, w, d, h=.14, color=None):
    P.box(place(z=h / 2), (w, d, h), color or (lambda p, n: stone(p, n, .8)), jitter=.01)


def headstone_round(name, seed, w=.62, h=1.0):
    P = Piece(name, seed)
    base_plinth(P, w + .16, .36)
    r = w / 2
    outline = [(-r, 0), (r, 0), (r, h - r)] + [(math.cos(a) * r, h - r + math.sin(a) * r) for a in [math.pi * k / 10 for k in range(1, 10)]] + [(-r, h - r)]
    P.slab(place(z=.12), outline, .14, lambda p, n: stone(p, n, .45))
    # A darker engraved panel and cross on the face.
    P.box(place(y=-.072, z=.12 + h * .42), (w * .62, .01, h * .36), flat(STONE_DARK))
    P.box(place(y=-.08, z=.12 + h * .72), (.04, .01, .2), flat(shade(STONE_DARK, .7)))
    P.box(place(y=-.08, z=.12 + h * .75), (.13, .01, .04), flat(shade(STONE_DARK, .7)))
    return P


def headstone_gothic(name, seed, w=.58, h=1.25):
    P = Piece(name, seed)
    base_plinth(P, w + .2, .38, .18)
    r = w / 2
    shoulder = h * .68
    arch = [(r * math.cos(t * math.pi / 2), shoulder + (h - shoulder) * math.sin(t * math.pi / 2)) for t in (.25, .5, .75)]
    outline = [(-r, 0), (r, 0), (r, shoulder)] + [(x * .9, z) for x, z in arch] + [(0, h)] + [(-x * .9, z) for x, z in reversed(arch)] + [(-r, shoulder)]
    P.slab(place(z=.17), outline, .16, lambda p, n: stone(p, n, .4, STONE_LIGHT))
    P.box(place(y=-.082, z=.17 + h * .5), (w * .55, .01, h * .5), flat(STONE_DARK))
    return P


def headstone_cross(name, seed, h=1.35):
    P = Piece(name, seed)
    base_plinth(P, .6, .45, .2)
    P.box(place(z=.27), (.44, .32, .14), lambda p, n: stone(p, n, .9), jitter=.01)
    col = lambda p, n: stone(p, n, .2, STONE_LIGHT)
    P.box(place(z=.34 + h / 2), (.13, .13, h), col, jitter=.004)
    P.box(place(z=.34 + h * .72), (.62, .13, .13), col, jitter=.004)
    return P


def headstone_celtic(name, seed, h=1.5):
    P = headstone_cross(name, seed, h)
    ring_z = .34 + h * .72
    pts = [Vector((math.cos(a) * .2, 0, ring_z + math.sin(a) * .2)) for a in [2 * math.pi * k / 16 for k in range(17)]]
    P.tube(pts, lambda t: .03, lambda t, a: STONE, sides=5, cap_start=False, cap_end=False)
    return P


def headstone_broken(name, seed, w=.66):
    P = Piece(name, seed)
    rng = P.rng
    base_plinth(P, w + .16, .36)
    # Jagged top where it snapped off, plus the broken piece lying at its foot.
    r = w / 2
    top = [(r - k * w / 6, .55 + rng.uniform(-.14, .14)) for k in range(7)]
    P.slab(place(z=.12, pitch=4), [(-r, 0), (r, 0)] + top[:-1] + [(-r, top[-1][1])], .14, lambda p, n: stone(p, n, .55))
    P.slab(place(y=-.45, z=.08, roll=-86, yaw=12), [(-r, 0), (r * .7, 0), (r * .8, .3), (0, .42), (-r, .3)], .13, lambda p, n: stone(p, n, .9))
    return P


def obelisk(name, seed, h=2.4):
    P = Piece(name, seed)
    col = lambda p, n: stone(p, n, .5, STONE_LIGHT)
    P.box(place(z=.12), (.95, .95, .24), lambda p, n: stone(p, n, .9), jitter=.01)
    P.box(place(z=.36), (.72, .72, .24), col, jitter=.01)
    P.box(place(z=.48 + h / 2), (.42, .42, h), col, taper=.62)
    pyramid = [Vector((x, y, .48 + h)) for x, y in ((-.13, -.13), (.13, -.13), (.13, .13), (-.13, .13))]
    ids = [P.vert(p, STONE_LIGHT) for p in pyramid]
    tip = P.vert((0, 0, .48 + h + .3), STONE_LIGHT)
    for i in range(4):
        P.face((ids[i], ids[(i + 1) % 4], tip))
    return P


# --- Graves -------------------------------------------------------------------------------------------------
def dirt(p, n):
    c = mix(DIRT_DARK, DIRT, .5 + .5 * noise.noise(p * 4.0))
    return mix(c, GRASS_BASE, max(0.0, n.z - .5) * .6 * (.5 + .5 * noise.noise(p * 2.0 + Vector((5, 5, 5)))))


def grave_mound(name, seed):
    P = Piece(name, seed)
    P.lump(Vector((0, 0, 0)), (.55, 1.1, .28), dirt, subdiv=2, rough=.12, seed=seed, flatten=-.05)
    return P


def grave_ledger(name, seed):
    P = Piece(name, seed)
    col = lambda p, n: stone(p, n, .6)
    P.box(place(z=.09), (1.0, 2.1, .18), col, jitter=.008)
    P.box(place(z=.2), (.84, 1.94, .06), lambda p, n: stone(p, n, .3, STONE_LIGHT), jitter=.004)
    P.box(place(y=-.35, z=.235), (.08, .7, .012), flat(STONE_DARK))
    P.box(place(y=-.5, z=.235), (.36, .08, .012), flat(STONE_DARK))
    return P


def open_grave(name, seed):
    """A freshly dug grave: a raised dirt rim around a dark pit (the ground stays flat, so the pit's depth is
    suggested by its inner walls and a coffin lid at the bottom), the dug-out pile and a shovel."""
    P = Piece(name, seed)
    rng = P.rng
    hw, hl, rim = .55, 1.1, .3
    for x, y, sx, sy in ((0, -hl - .15, 2 * hw + .6, .3), (0, hl + .15, 2 * hw + .6, .3), (-hw - .15, 0, .3, 2 * hl), (hw + .15, 0, .3, 2 * hl)):
        P.lump(Vector((x, y, .02)), (sx / 2 + .06, sy / 2 + .06, rim), dirt, subdiv=2, rough=.18, seed=seed + int(x * 10 + y * 7), flatten=-.02)
    # Inner walls from the rim down to the floor, darkening with depth, and the floor itself.
    corners = [(-hw, -hl), (hw, -hl), (hw, hl), (-hw, hl)]
    top = [P.vert((x, y, rim * .9), DIRT_DARK) for x, y in corners]
    bottom = [P.vert((x * .92, y * .96, .015), PIT) for x, y in corners]
    for i in range(4):
        j = (i + 1) % 4
        P.face((top[i], top[j], bottom[j], bottom[i]))
    P.face(tuple(reversed(bottom)))
    P.box(place(y=.15, z=.05, yaw=rng.uniform(-4, 4)), (.62, 1.5, .06), flat(WOOD_DARK))
    # The pile and the shovel stuck in it.
    P.lump(Vector((hw + 1.0, .2, 0)), (.6, .9, .5), dirt, subdiv=2, rough=.25, seed=seed + 9, flatten=-.02)
    P.tube([Vector((hw + 1.0, .1, .35)), Vector((hw + 1.25, .05, 1.35))], lambda t: .025, lambda t, a: WOOD, sides=5)
    P.box(place(hw + .97, .11, .28, pitch=-14), (.2, .03, .3), iron)
    return P


def sarcophagus(name, seed):
    P = Piece(name, seed)
    col = lambda p, n: stone(p, n, .35)
    for x in (-.42, .42):
        for y in (-.95, .95):
            P.box(place(x, y, .08), (.22, .22, .16), col)
    P.box(place(z=.5), (1.0, 2.2, .7), col, jitter=.01)
    P.box(place(z=.9), (1.12, 2.32, .12), lambda p, n: stone(p, n, .2, STONE_LIGHT), jitter=.01)
    P.box(place(z=1.0), (.92, 2.1, .1), lambda p, n: stone(p, n, .2, STONE_LIGHT), taper=.9)
    P.box(place(z=1.07), (.1, 1.2, .05), flat(STONE_LIGHT))
    P.box(place(y=-.25, z=1.07), (.55, .1, .05), flat(STONE_LIGHT))
    P.box(place(y=-1.105, z=.5), (.5, .01, .35), flat(STONE_DARK))
    return P


# --- Buildings ------------------------------------------------------------------------------------------------
def gable(P, M, width, depth, rise, color, overhang=.25):
    """A pitched roof over a width x depth box, ridge along Y, eaves at local z 0."""
    for side in (-1, 1):
        run = width / 2 + overhang
        length = math.hypot(run, rise)
        angle = math.degrees(math.atan2(rise, run))
        P.box(M @ place(side * run / 2, 0, rise / 2, pitch=side * angle), (length + .05, depth + overhang * 2, .16), color, jitter=.01)
    for y in (-(depth / 2), depth / 2):
        P.slab(M @ place(0, y, 0), [(-width / 2, 0), (width / 2, 0), (0, rise)], .12, color)


def mausoleum_gothic(name, seed):
    P = Piece(name, seed)
    wall = lambda p, n: stone(p, n, .6)
    P.box(place(z=.15), (3.6, 4.6, .3), lambda p, n: stone(p, n, .9), jitter=.02)
    for k in range(3):  # front steps
        P.box(place(0, -2.45 - k * .3, .1 - k * .03), (1.6 + k * .2, .32, .2 - k * .06), lambda p, n: stone(p, n, .9))
    P.box(place(z=1.6), (3.0, 4.0, 2.6), wall, jitter=.015)
    for x in (-1.45, 1.45):
        for y in (-1.95, 1.95):
            P.box(place(x, y, 1.6), (.4, .4, 2.7), lambda p, n: stone(p, n, .6, STONE_LIGHT))
    P.box(place(z=2.97), (3.4, 4.4, .2), lambda p, n: stone(p, n, .2, STONE_LIGHT))
    gable(P, place(z=3.07), 3.3, 4.3, 1.5, lambda p, n: stone(p, n, .25, STONE_DARK))
    # Door: a dark iron door in a pointed frame, a carved cross above.
    frame = [(-.62, 0), (.62, 0), (.62, 1.6), (.35, 2.0), (0, 2.2), (-.35, 2.0), (-.62, 1.6)]
    P.slab(place(0, -2.02, .3), frame, .06, lambda p, n: stone(p, n, .3, STONE_LIGHT))
    P.slab(place(0, -2.06, .3), [(x * .8, z * .9) for x, z in frame], .04, iron)
    P.box(place(0, -2.09, 1.3), (.04, .01, 1.5), flat(IRON_RUST))
    P.box(place(0, -2.04, 3.55), (.1, .08, .5), flat(STONE_LIGHT))
    P.box(place(0, -2.04, 3.65), (.3, .08, .1), flat(STONE_LIGHT))
    # A finial cross on the ridge.
    P.box(place(0, -2.1, 4.9), (.1, .1, .7), flat(STONE_LIGHT))
    P.box(place(0, -2.1, 5.0), (.36, .1, .1), flat(STONE_LIGHT))
    return P


def mausoleum_dome(name, seed):
    P = Piece(name, seed)
    P.box(place(z=.15), (4.0, 4.0, .3), lambda p, n: stone(p, n, .9), jitter=.02)
    for k in range(3):
        P.box(place(0, -2.15 - k * .3, .1 - k * .03), (1.5 + k * .2, .32, .2 - k * .06), lambda p, n: stone(p, n, .9))
    P.box(place(z=1.55), (3.0, 3.0, 2.5), lambda p, n: stone(p, n, .6), jitter=.015)
    for x in (-1.7, 1.7):
        for y in (-1.7, 1.7):
            P.tube([Vector((x, y, .3)), Vector((x, y, 2.8))], lambda t: .2, lambda t, a: mix(STONE, STONE_LIGHT, .5 + .3 * math.sin(a * 4)), sides=10)
            P.box(place(x, y, .4), (.5, .5, .2), flat(STONE))
    P.box(place(z=2.92), (4.1, 4.1, .25), lambda p, n: stone(p, n, .2, STONE_LIGHT))
    P.box(place(z=3.15), (3.3, 3.3, .22), lambda p, n: stone(p, n, .2, STONE))
    P.lump(Vector((0, 0, 3.2)), (1.55, 1.55, 1.25), lambda p, n: stone(p, n, .1, STONE_DARK), subdiv=3, rough=.03, seed=seed, flatten=0)
    P.tube([Vector((0, 0, 4.3)), Vector((0, 0, 5.1))], lambda t: .12 * (1 - t * .8), lambda t, a: STONE_LIGHT, sides=6)
    arch = [(-.55, 0), (.55, 0), (.55, 1.5)] + [(math.cos(a) * .55, 1.5 + math.sin(a) * .55) for a in [math.pi * k / 6 for k in range(1, 6)]] + [(-.55, 1.5)]
    P.slab(place(0, -1.52, .3), arch, .06, lambda p, n: stone(p, n, .3, STONE_LIGHT))
    P.slab(place(0, -1.56, .3), [(x * .8, z * .92) for x, z in arch], .04, iron)
    return P


def wall_section(name, seed, length=4.0):
    """A low stone wall with an iron railing on top; the kit's fence around the graveyard."""
    P = Piece(name, seed)
    P.box(place(z=.5), (length, .55, 1.0), lambda p, n: stone(p, n, .9), jitter=.015)
    P.box(place(z=1.06), (length + .04, .66, .12), lambda p, n: stone(p, n, .3, STONE_LIGHT), jitter=.01)
    bars = int(length / .15)
    for k in range(bars):
        x = -length / 2 + (k + .5) * length / bars
        P.box(place(x, 0, 1.72), (.028, .028, 1.2), iron)
        tip = [P.vert((x - .03, -.03, 2.32), IRON), P.vert((x + .03, -.03, 2.32), IRON), P.vert((x + .03, .03, 2.32), IRON), P.vert((x - .03, .03, 2.32), IRON)]
        top = P.vert((x, 0, 2.45), IRON)
        for i in range(4):
            P.face((tip[i], tip[(i + 1) % 4], top))
    for z in (1.25, 2.18):
        P.box(place(0, 0, z), (length, .04, .05), iron)
    return P


def wall_pillar(name, seed, height=2.7):
    P = Piece(name, seed)
    P.box(place(z=height / 2), (.75, .75, height), lambda p, n: stone(p, n, .9), jitter=.012)
    P.box(place(z=height + .08), (.9, .9, .16), lambda p, n: stone(p, n, .2, STONE_LIGHT))
    P.box(place(z=height + .22), (.5, .5, .14), flat(STONE))
    P.lump(Vector((0, 0, height + .5)), (.24, .24, .24), lambda p, n: stone(p, n, .1, STONE_LIGHT), subdiv=2, rough=.02, seed=seed)
    return P


def iron_gate(name, seed):
    """The entrance: two tall pillars, an iron arch with scrollwork and the two gate leaves standing open (toward -Y)."""
    P = Piece(name, seed)
    for side in (-1, 1):
        x = side * 2.0
        P.box(place(x, 0, 1.6), (.9, .9, 3.2), lambda p, n: stone(p, n, .9), jitter=.012)
        P.box(place(x, 0, 3.28), (1.05, 1.05, .18), lambda p, n: stone(p, n, .2, STONE_LIGHT))
        P.lump(Vector((x, 0, 3.7)), (.3, .3, .3), lambda p, n: stone(p, n, .1, STONE_LIGHT), subdiv=2, rough=.02, seed=seed + side)
        # An open leaf: hinged at the pillar, swung 70 degrees outward.
        M = place(side * 1.55, -.05, 0, yaw=side * 70)
        P.box(M @ place(-side * .78, 0, .15), (1.5, .05, .06), iron)
        P.box(M @ place(-side * .78, 0, 2.5), (1.5, .05, .06), iron)
        for k in range(9):
            P.box(M @ place(-side * (.08 + k * .175), 0, 1.35), (.03, .03, 2.45), iron)
        P.box(M @ place(-side * .78, 0, 1.3), (1.5, .04, .04), iron)
    arc = [Vector((-1.55 + 3.1 * t, 0, 3.0 + .75 * math.sin(math.pi * t))) for t in [k / 12 for k in range(13)]]
    P.tube(arc, lambda t: .05, lambda t, a: IRON, sides=5)
    P.tube([Vector((-1.55, 0, 2.9)), Vector((1.55, 0, 2.9))], lambda t: .035, lambda t, a: IRON, sides=5)
    for k in range(5):  # scrolls between the bar and the arch
        x = -1.1 + k * .55
        loop = [Vector((x + .14 * math.cos(a), 0, 3.25 + .14 * math.sin(a))) for a in [2 * math.pi * j / 10 for j in range(11)]]
        P.tube(loop, lambda t: .018, lambda t, a: IRON, sides=4, cap_start=False, cap_end=False)
    P.box(place(0, 0, 3.9), (.05, .05, .5), iron)
    P.box(place(0, 0, 3.95), (.26, .05, .05), iron)
    return P


def crypt_exit(name, seed):
    """The way out: a crypt entrance with steps down into a doorway filled with pale ghost light (slot 1)."""
    P = Piece(name, seed)
    wall = lambda p, n: stone(p, n, .9)
    for side in (-1, 1):
        P.box(place(side * 1.75, 0, 2.2), (1.5, 1.6, 4.4), wall, jitter=.02)
        P.box(place(side * 1.3, -.85, 2.0), (.45, .45, 4.0), lambda p, n: stone(p, n, .5, STONE_LIGHT))
    arch = [(-1.0, 0), (1.0, 0), (1.0, 2.6), (.75, 3.15), (.4, 3.45), (0, 3.6), (-.4, 3.45), (-.75, 3.15), (-1.0, 2.6)]
    # The stone above the doorway follows the arch (two spandrels), so the opening stays open; a moulding runs round it.
    spandrel = [(-1.05, 2.6), (-1.0, 2.6), (-.75, 3.15), (-.4, 3.45), (0, 3.6), (0, 4.4), (-1.05, 4.4)]
    P.slab(place(0, -.6, 0), spandrel, .4, wall)
    P.slab(place(0, -.6, 0), [(-x, z) for x, z in reversed(spandrel)], .4, wall)
    rim = [(1.0, 0), (1.0, 2.6), (.75, 3.15), (.4, 3.45), (0, 3.6), (-.4, 3.45), (-.75, 3.15), (-1.0, 2.6), (-1.0, 0)]
    P.tube([Vector((x * 1.06, -.84, z * 1.02)) for x, z in rim], lambda t: .09, lambda t, a: mix(STONE, STONE_LIGHT, .6), sides=5)
    P.box(place(0, 0, 4.55), (5.2, 1.8, .3), lambda p, n: stone(p, n, .2, STONE_LIGHT), jitter=.015)
    gable(P, place(z=4.7), 5.0, 1.7, 1.3, lambda p, n: stone(p, n, .25, STONE_DARK))
    # A skull on the keystone and a cross on top.
    P.lump(Vector((0, -.92, 3.85)), (.16, .1, .17), flat(BONE), subdiv=2, rough=.08, seed=seed)
    for x in (-.06, .06):
        P.lump(Vector((x, -1.0, 3.88)), (.04, .02, .04), flat(PIT), subdiv=1, rough=0, seed=seed)
    P.box(place(0, -.8, 6.25), (.14, .14, .9), flat(STONE_LIGHT))
    P.box(place(0, -.8, 6.4), (.5, .14, .14), flat(STONE_LIGHT))
    # Steps down into the dark and the glowing doorway at the back.
    for k in range(4):
        P.box(place(0, -.4 + k * .3, .02 - k * .02), (2.0, .3, .04), flat(shade(STONE_DARK, 1 - k * .2)))
    P.slab(place(0, .7, 0), arch, .05, flat(GHOST), slot=1)
    for side in (-1, 1):  # dark inner walls framing the light
        P.box(place(side * 1.0, 0, 1.8), (.06, 1.4, 3.6), flat(PIT))
    return P


# --- Plants, trees and garden -------------------------------------------------------------------------------
def rose_blossoms(P, points, seed):
    rng = P.rng
    for i, p in enumerate(points):
        s = rng.uniform(.055, .085)
        P.lump(p, (s, s, s * .8), lambda q, n: mix(ROSE_DARK, ROSE, .35 + .65 * max(0.0, n.z + .3)), subdiv=1, rough=.35, seed=seed + i, sway=.35)


def rose_bush(name, seed, size=1.0):
    P = Piece(name, seed)
    rng = P.rng
    leaves = lambda p, n: mix(LEAF_DARK, LEAF, .45 + .55 * n.z)
    blobs = []
    for k in range(6):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0, .3) * size
        c = Vector((math.cos(a) * r, math.sin(a) * r, rng.uniform(.3, .75) * size))
        s = rng.uniform(.28, .4) * size
        P.lump(c, (s, s, s * .85), leaves, subdiv=1, rough=.4, seed=seed + k, sway=.2)
        blobs.append((c, s))
    for k in range(5):  # thorny canes poking out
        a = rng.uniform(0, 2 * math.pi)
        d = Vector((math.cos(a) * .5, math.sin(a) * .5, 1.0)).normalized()
        P.tube([Vector((0, 0, 0)), d * .6 * size, d * 1.05 * size + Vector((math.cos(a) * .15, math.sin(a) * .15, -.1))],
               lambda t: .018 * (1 - t * .6), lambda t, a2: LEAF_DARK, sides=4, sway=lambda t: t * .5)
    spots = []
    for k in range(int(26 * size)):
        c, s = blobs[rng.randrange(len(blobs))]
        d = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(.1, 1))).normalized()
        spots.append(c + d * s * .95)
    rose_blossoms(P, spots, seed + 50)
    return P


def roses_laid(name, seed):
    """A small bunch of roses laid on a grave."""
    P = Piece(name, seed)
    rng = P.rng
    tips = []
    for k in range(5):
        a = rng.uniform(-.5, .5)
        end = Vector((math.sin(a) * .4, -math.cos(a) * .4 + .2, .03))
        P.tube([Vector((0, .2, .02)), end], lambda t: .008, lambda t, a2: LEAF, sides=4, cap_start=False)
        tips.append(end + Vector((0, 0, .03)))
    rose_blossoms(P, tips, seed)
    P.lump(Vector((0, .05, .04)), (.07, .1, .04), flat(LEAF_DARK), subdiv=1, rough=.3, seed=seed)
    return P


def rose_arch(name, seed):
    """An iron arch over a garden path, overgrown with roses."""
    P = Piece(name, seed)
    rng = P.rng
    for y in (-.3, .3):
        # Near-vertical sides and a rounded top: height = 2.6 * sin^0.35.
        arc = [Vector((-1.2 + 2.4 * t, y, 2.6 * math.sin(math.pi * t) ** .35)) for t in [k / 14 for k in range(15)]]
        P.tube(arc, lambda t: .03, lambda t, a: IRON, sides=5)
    spots = []
    for k in range(14):
        t = k / 13
        x = -1.2 + 2.4 * t
        z = 2.6 * math.sin(math.pi * t) ** .35
        c = Vector((x, rng.uniform(-.3, .3), z))
        s = rng.uniform(.2, .3)
        P.lump(c, (s, s * 1.4, s), lambda p, n: mix(LEAF_DARK, LEAF, .5 + .5 * n.z), subdiv=1, rough=.4, seed=seed + k, sway=.15)
        spots += [c + Vector((rng.uniform(-.2, .2), rng.uniform(-.35, .35), rng.uniform(-.1, .2))) for _ in range(2)]
    rose_blossoms(P, spots, seed + 30)
    return P


def hedge(name, seed, length=3.0):
    P = Piece(name, seed)
    rng = P.rng
    col = lambda p, n: mix(LEAF_DARK, LEAF, .35 + .5 * n.z + .15 * noise.noise(p * 3))
    # A clipped box hedge, 1.15 m tall, its top and sides softened by overlapping clumps of leaves.
    P.box(place(z=.55), (length, .72, 1.1), col, jitter=.04)
    for k in range(int(length / .35)):
        x = -length / 2 + .18 + k * .35 + rng.uniform(-.06, .06)
        for y in (-.18, .18):
            s = rng.uniform(.2, .3)
            P.lump(Vector((x, y + rng.uniform(-.05, .05), 1.08 + rng.uniform(-.03, .04))), (s, s * .9, s * .55), col, subdiv=2, rough=.25, seed=seed + k * 2 + (y > 0), sway=.06)
        side = rng.choice((-1, 1))
        P.lump(Vector((x, side * .36, rng.uniform(.3, .8))), (.18, .08, .2), col, subdiv=1, rough=.3, seed=seed + 90 + k)
    return P


def dead_oak(name, seed, height=9.0):
    """The big, gnarled dead oak at the crossing: flared roots, a split trunk and a wide crown of bare branches."""
    P = Piece(name, seed)
    rng = P.rng
    grey = lambda t, a: mix(BARK, shade(BARK, .55), .5 + .5 * math.sin(a * 3 + t * 5))
    for k in range(6):  # roots
        a = 2 * math.pi * k / 6 + rng.uniform(-.2, .2)
        d = Vector((math.cos(a), math.sin(a), 0))
        # Thick flares that dive into the ground instead of trailing off into thin spikes.
        P.tube([d * .25 + Vector((0, 0, .8)), d * .85 + Vector((0, 0, .2)), d * 1.25 + Vector((0, 0, -.25))],
               lambda t: .38 * (1 - t * .45), grey, sides=6, wobble=.1, cap_end=False)
    trunk = [Vector((.2 * math.sin(t * 4), .15 * math.cos(t * 3), height * .42 * t)) for t in [k / 5 for k in range(6)]]
    P.tube(trunk, lambda t: .7 * (1 - t * .35), grey, sides=10, wobble=.12, cap_start=False)

    def branch(start, direction, length, radius, depth):
        path, p, d = [start], start.copy(), direction.normalized()
        for s in range(4):
            d = (d + Vector((rng.uniform(-.4, .4), rng.uniform(-.4, .4), rng.uniform(-.25, .2)))).normalized()
            p = p + d * length / 4
            path.append(p.copy())
        P.tube(path, lambda t: radius * (1 - t * .7), grey, sides=max(4, 8 - depth * 2), wobble=.08, sway=lambda t: min(1.0, depth * .25 + t * .3))
        if depth < 3:
            for _ in range(rng.randint(2, 3)):
                i = rng.randint(2, 4)
                a = rng.uniform(0, 2 * math.pi)
                branch(path[i], d + Vector((math.cos(a), math.sin(a), rng.uniform(.1, .6))) * .9, length * rng.uniform(.5, .7), radius * .45, depth + 1)

    top = trunk[-1]
    for k in range(4):
        a = 2 * math.pi * k / 4 + rng.uniform(-.3, .3)
        branch(top, Vector((math.cos(a), math.sin(a), rng.uniform(.6, 1.1))), height * .42, .4, 1)
    return P


def well(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    for k in range(14):  # the stone ring
        a = 2 * math.pi * k / 14
        P.box(place(math.cos(a) * 1.0, math.sin(a) * 1.0, .45, yaw=math.degrees(a) + 90), (.5, .36, .9), lambda p, n: stone(p, n, .9), jitter=.03)
    P.box(place(z=.3), (1.5, 1.5, .02), flat(PIT))
    for side in (-1, 1):
        P.tube([Vector((side * 1.05, 0, .2)), Vector((side * 1.05, 0, 2.6))], lambda t: .08, lambda t, a: WOOD_DARK, sides=6)
    P.tube([Vector((-1.15, 0, 2.2)), Vector((1.15, 0, 2.2))], lambda t: .06, lambda t, a: WOOD, sides=6)
    gable(P, place(z=2.55), 2.2, 1.2, .8, lambda p, n: mix(WOOD_DARK, WOOD, .5 + .5 * noise.noise(p * 5)), overhang=.2)
    P.tube([Vector((0, 0, 2.15)), Vector((0, 0, 1.3))], lambda t: .008, lambda t, a: WOOD, sides=3, sway=lambda t: t * .3)
    P.tube([Vector((0, 0, 1.3)), Vector((0, 0, 1.0))], lambda t: .14 - t * .02, lambda t, a: WOOD_DARK, sides=8, sway=lambda t: .35)
    return P


def statue_mourner(name, seed, wings=False):
    """A hooded mourning figure on a pedestal, head bowed, hands folded; optionally with angel wings."""
    P = Piece(name, seed)
    pale = lambda p, n: stone(p, n, .25, STONE_LIGHT)
    P.box(place(z=.55), (.95, .95, 1.1), lambda p, n: stone(p, n, .9), jitter=.01)
    P.box(place(z=1.15), (1.1, 1.1, .12), pale)
    robe = [Vector((0, 0, 1.2)), Vector((0, .02, 1.7)), Vector((0, .04, 2.3)), Vector((0, 0, 2.75))]
    P.tube(robe, lambda t: .36 - t * .2 if t < .9 else .17, lambda t, a: mix(STONE_LIGHT, STONE, .5 + .5 * math.sin(a * 6)), sides=12, wobble=.05)
    P.lump(Vector((0, -.03, 2.92)), (.2, .22, .24), pale, subdiv=2, rough=.08, seed=seed)       # hood
    P.lump(Vector((0, -.16, 2.86)), (.11, .06, .13), flat(shade(STONE_DARK, .6)), subdiv=1, rough=.05, seed=seed)  # shadowed face
    P.lump(Vector((0, -.24, 2.35)), (.14, .1, .1), pale, subdiv=1, rough=.1, seed=seed + 1)    # folded hands
    if wings:
        feather = [(0, 0), (.25, .1), (.55, .45), (.75, .95), (.7, 1.35), (.45, 1.05), (.3, .75), (.1, .45)]
        for side in (-1, 1):
            outline = [(side * x, z) for x, z in feather]
            if side < 0:
                outline = list(reversed(outline))
            P.slab(place(side * .12, .22, 2.0, yaw=side * -20), outline, .06, pale)
    return P


def candles(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    P.lump(Vector((0, 0, 0)), (.2, .16, .04), flat(WAX), subdiv=1, rough=.3, seed=seed)
    for k in range(rng.randint(3, 5)):
        x, y = rng.uniform(-.14, .14), rng.uniform(-.1, .1)
        h = rng.uniform(.08, .26)
        P.tube([Vector((x, y, 0)), Vector((x, y, h))], lambda t: .025, lambda t, a: WAX, sides=6)
        P.lump(Vector((x, y, h + .035)), (.014, .014, .035), flat(GHOST), subdiv=1, rough=.1, seed=seed + k, slot=1, sway=.2)
    return P


def ghost_lantern(name, seed):
    """An iron post with a hanging lantern holding a pale ghost-light."""
    P = Piece(name, seed)
    P.box(place(z=.1), (.3, .3, .2), lambda p, n: stone(p, n, .9))
    P.tube([Vector((0, 0, .15)), Vector((0, 0, 2.4))], lambda t: .045, lambda t, a: IRON, sides=6)
    P.tube([Vector((0, 0, 2.3)), Vector((.3, 0, 2.45)), Vector((.5, 0, 2.35))], lambda t: .025, lambda t, a: IRON, sides=4)
    M = place(.5, 0, 1.95)
    P.box(M @ place(z=.02), (.2, .2, .04), iron)
    P.box(M @ place(z=.3), (.24, .24, .05), iron)
    P.lump(Vector((.5, 0, 2.35)), (.1, .1, .08), iron, subdiv=1, rough=0, seed=seed)
    for x, y in ((-.09, -.09), (.09, -.09), (.09, .09), (-.09, .09)):
        P.box(M @ place(x, y, .16), (.02, .02, .28), iron)
    P.lump(Vector((.5, 0, 2.11)), (.06, .06, .09), flat(GHOST), subdiv=1, rough=.15, seed=seed, slot=1)
    return P


def bones(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    P.lump(Vector((0, 0, .09)), (.1, .12, .09), flat(BONE), subdiv=2, rough=.08, seed=seed)
    for x in (-.04, .04):
        P.lump(Vector((x, -.11, .1)), (.025, .01, .025), flat(PIT), subdiv=1, rough=0, seed=seed)
    for k in range(4):
        a = rng.uniform(0, math.pi)
        c = Vector((rng.uniform(-.4, .4), rng.uniform(-.3, .4), .025))
        d = Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(.14, .24)
        P.tube([c - d, c + d], lambda t: .02, lambda t, a2: BONE, sides=5)
        for e in (c - d, c + d):
            P.lump(e, (.035, .035, .03), flat(BONE), subdiv=1, rough=.1, seed=seed + k)
    return P


def grass(name, seed, blades=14, tall=1.0):
    P = Piece(name, seed)
    rng = P.rng
    for b in range(blades):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0, .18)
        base = Vector((math.cos(a) * r, math.sin(a) * r, 0))
        lean = Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(.1, .35)
        H = rng.uniform(.3, .6) * tall
        w = rng.uniform(.025, .045)
        side = Vector((-math.sin(a + 1.2), math.cos(a + 1.2), 0))
        tip_col = shade(GRASS_TIP, rng.uniform(.7, 1.2))
        pts = []
        for s in range(4):
            t = s / 3
            c = base + lean * t * t + Vector((0, 0, H * t))
            col = mix(GRASS_BASE, tip_col, t)
            pts.append((P.vert(c + side * w * (1 - t), col, t), P.vert(c - side * w * (1 - t), col, t)) if s < 3 else (P.vert(c, col, 1.0),))
        for s in range(2):
            (a1, b1), (a2, b2) = pts[s], pts[s + 1]
            P.face((a1, b1, b2, a2))
        (a1, b1), (tipv,) = pts[2], pts[3]
        P.face((a1, b1, tipv))
    return P


def broken_column(name, seed):
    P = Piece(name, seed)
    P.box(place(z=.15), (.9, .9, .3), lambda p, n: stone(p, n, .9), jitter=.02)
    P.tube([Vector((0, 0, .3)), Vector((0, 0, 1.6))], lambda t: .3, lambda t, a: mix(STONE, STONE_LIGHT, .5 + .4 * math.sin(a * 5)), sides=10, wobble=.04)
    P.tube([Vector((.35, -.9, .3)), Vector((.5, -1.9, .3))], lambda t: .29, lambda t, a: mix(STONE_DARK, STONE, .5 + .4 * math.sin(a * 5)), sides=10, wobble=.04)
    return P


pieces = [
    headstone_round("SM_HeadstoneRound", 1), headstone_round("SM_HeadstoneRoundSmall", 2, .5, .75),
    headstone_gothic("SM_HeadstoneGothic", 3), headstone_cross("SM_HeadstoneCross", 4), headstone_celtic("SM_HeadstoneCeltic", 5),
    headstone_broken("SM_HeadstoneBroken", 6), obelisk("SM_Obelisk", 7),
    grave_mound("SM_GraveMound", 8), grave_ledger("SM_GraveLedger", 9), open_grave("SM_OpenGrave", 10), sarcophagus("SM_Sarcophagus", 11),
    mausoleum_gothic("SM_MausoleumGothic", 12), mausoleum_dome("SM_MausoleumDome", 13),
    wall_section("SM_GraveWall", 14), wall_pillar("SM_GraveWallPillar", 15), iron_gate("SM_GraveGate", 16), crypt_exit("SM_CryptExit", 17),
    rose_bush("SM_RoseBush", 18), rose_bush("SM_RoseBushSmall", 19, .7), roses_laid("SM_RosesLaid", 20), rose_arch("SM_RoseArch", 21),
    hedge("SM_Hedge", 22), dead_oak("SM_DeadOak", 23), well("SM_Well", 24),
    statue_mourner("SM_StatueMourner", 25), statue_mourner("SM_StatueAngel", 26, wings=True),
    candles("SM_Candles", 27), ghost_lantern("SM_GhostLantern", 28), bones("SM_Bones", 29),
    grass("SM_GraveGrass", 30), grass("SM_GraveGrassTall", 31, 18, 1.6), broken_column("SM_BrokenColumn", 32),
]
bpy.ops.wm.read_factory_settings(use_empty=True)
materials = (bpy.data.materials.new("Kit"), bpy.data.materials.new("Glow"))
objects = [p.build(materials) for p in pieces]
export(objects, OUT)
if PREVIEW:
    preview(objects, PREVIEW)
print("GRAVEYARD KIT DONE")
