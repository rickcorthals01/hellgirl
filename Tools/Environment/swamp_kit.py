# Blender: generates the swamp kit for World II: dead swamp trees with hanging moss, the glowing zombie arm, lily pads,
# the rotten boardwalk, reeds, drowned stumps, mud islands, the Frog King's giant lily pad, the exit lantern and the
# ripple ring used for splashes. Low-poly with vertex colours like the other kits (alpha = sway), slot 1 glows.
# Run through build_swamp_kit.ps1, or:
#   blender -b --factory-startup -P swamp_kit.py -- <output folder> [preview.png]
import bpy, math, os, sys
from mathutils import Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from kit_common import Piece, mix, shade, place, export, preview

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = args[1] if len(args) > 1 else None

BARK, BARK_DARK = (.075, .068, .058), (.035, .032, .028)
MOSS, MOSS_LIGHT = (.09, .11, .075), (.2, .22, .15)
WOOD, WOOD_DARK, WOOD_ROT = (.16, .12, .08), (.07, .05, .035), (.1, .11, .07)
MUD, MUD_DARK, MUD_MOSS = (.075, .06, .045), (.035, .028, .022), (.06, .08, .045)
LILY, LILY_DARK, LILY_EDGE = (.035, .075, .045), (.012, .03, .02), (.06, .1, .05)
REED, REED_TIP, CATTAIL = (.06, .09, .05), (.14, .15, .08), (.13, .07, .04)
SOUL, SOUL_BRIGHT, SOUL_DEEP = (.2, .58, 1.0), (.55, .85, 1.0), (.06, .28, .75)
LANTERN = (1.0, .82, .55)


def bark(t, a):
    return mix(BARK, BARK_DARK, .5 + .5 * math.sin(a * 3 + t * 7))


def swamp_tree(name, seed, height, lean=.5, broken=False):
    """A dead swamp tree: flared roots in the mud, a twisted trunk, crooked bare branches hung with moss."""
    P = Piece(name, seed)
    rng = P.rng
    for k in range(7):
        a = 2 * math.pi * k / 7 + rng.uniform(-.25, .25)
        d = Vector((math.cos(a), math.sin(a), 0))
        # Buttress roots: short and steep, sinking into the mud rather than spreading like spokes.
        P.tube([d * .2 + Vector((0, 0, 1.0)), d * .6 + Vector((0, 0, .35)), d * .9 + Vector((0, 0, -.3))],
               lambda t: .32 * (1 - t * .35), bark, sides=6, wobble=.12, cap_end=False)
    bend = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * lean
    top = height * (.7 if broken else 1.0)
    trunk = [Vector((.25 * math.sin(t * 6 + seed), .2 * math.cos(t * 5), top * t)) + bend * t * t for t in [k / 7 for k in range(8)]]
    P.tube(trunk, lambda t: .5 * (1 - t * .75) + .04, bark, sides=9, wobble=.14, cap_start=False, cap_end=broken)
    moss = lambda p, n: mix(MOSS, MOSS_LIGHT, .5 + .5 * noise.noise(p * 4))
    hangs = []

    def branch(start, direction, length, radius, depth):
        path, p, d = [start], start.copy(), direction.normalized()
        for s in range(4):
            d = (d + Vector((rng.uniform(-.45, .45), rng.uniform(-.45, .45), rng.uniform(-.35, .15)))).normalized()
            p = p + d * length / 4
            path.append(p.copy())
        P.tube(path, lambda t: radius * (1 - t * .75), bark, sides=max(4, 7 - depth * 2), wobble=.08, sway=lambda t: min(1.0, depth * .2 + t * .3))
        hangs.extend(path[1:])
        if depth < 2:
            for _ in range(rng.randint(1, 3)):
                a = rng.uniform(0, 2 * math.pi)
                branch(path[rng.randint(2, 3)], d + Vector((math.cos(a), math.sin(a), rng.uniform(-.1, .5))), length * .6, radius * .5, depth + 1)

    for k in range(rng.randint(3, 5)):
        i = rng.randint(4, 7) if not broken else rng.randint(3, 6)
        a = rng.uniform(0, 2 * math.pi)
        branch(trunk[i], Vector((math.cos(a), math.sin(a), rng.uniform(.1, .7))), height * rng.uniform(.25, .38), .2, 0)
    # Moss strands: thin ribbons hanging straight down from the branches.
    for p in hangs:
        if rng.random() > .55:
            continue
        L = rng.uniform(.6, 1.8)
        w = rng.uniform(.06, .12)
        side = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * w
        ids = []
        for s in range(4):
            t = s / 3
            c = p + Vector((0, 0, -L * t)) + Vector((math.sin(t * 3 + p.x) * .08, 0, 0))
            col = mix(MOSS_LIGHT, MOSS, t)
            ids.append((P.vert(c + side * (1 - t * .6), col, .3 + t * .7), P.vert(c - side * (1 - t * .6), col, .3 + t * .7)))
        for s in range(3):
            (a1, b1), (a2, b2) = ids[s], ids[s + 1]
            P.face((a1, b1, b2, a2))
    return P


def zombie_arm(name, seed):
    """A drowned arm reaching up out of the water, all glowing soul-blue (slot 1). The fingers carry sway weight so the
    material makes them flex; the level rises, sways and sinks the whole arm."""
    P = Piece(name, seed)
    rng = P.rng
    glow = lambda t, a: mix(SOUL_DEEP, SOUL, .3 + .7 * t)
    elbow = Vector((.08, 0, .15))
    wrist = Vector((.02, -.06, 1.0))
    P.tube([Vector((0, 0, -.6)), elbow, (elbow + wrist) / 2 + Vector((.03, 0, 0)), wrist], lambda t: .1 - t * .03 + (.02 if .2 < t < .4 else 0),
           glow, sides=7, wobble=.12, slot=1)
    # Ragged cuff of a sleeve round the forearm.
    for k in range(8):
        a = 2 * math.pi * k / 8
        base = Vector((math.cos(a) * .09, math.sin(a) * .09, .32))
        P.tube([base, base + Vector((math.cos(a) * .05, math.sin(a) * .05, rng.uniform(-.18, -.08)))], lambda t: .025 * (1 - t), lambda t, a2: SOUL_DEEP, sides=3, slot=1)
    palm = wrist + Vector((0, -.03, .1))
    P.lump(palm, (.09, .045, .1), lambda p, n: SOUL, subdiv=1, rough=.15, seed=seed, slot=1, sway=.1)
    for f in range(5):
        thumb = f == 0
        x = -.06 + f * .03 if not thumb else -.075
        root = palm + Vector((x, -.01, .06 if not thumb else -.01))
        spread = (f - 2) * .12
        pts, p = [root], root.copy()
        d = Vector((spread + (-.6 if thumb else 0), -.25, 1)).normalized()
        L = (.09 if not thumb else .07) * (1.1 if f == 2 else 1)
        for s in range(3):
            d = (d + Vector((0, -.55, -.15))).normalized()  # curl into a claw
            p = p + d * L
            pts.append(p.copy())
        P.tube(pts, lambda t: .024 * (1 - t * .5), lambda t, a: mix(SOUL, SOUL_BRIGHT, t), sides=4, sway=lambda t: .3 + .7 * t, slot=1)
    # Half as big again as a real arm, so it reads from the play camera: 1.65 m out of the water when risen.
    P.v = [v * 1.5 for v in P.v]
    return P


def lily_pad(name, seed, radius, glow_veins=False):
    """A dark, slightly cupped lily pad with its notch."""
    P = Piece(name, seed)
    rng = P.rng
    n = 16
    notch = rng.uniform(0, 2 * math.pi)
    centre = P.vert((0, 0, .02), LILY_DARK, .15)
    ring = []
    for k in range(n + 1):
        a = notch + .35 + (2 * math.pi - .7) * k / n
        r = radius * rng.uniform(.95, 1.02)
        ring.append(P.vert((math.cos(a) * r, math.sin(a) * r, .035 + rng.uniform(0, .02)), mix(LILY, LILY_EDGE, rng.random() * .5), .15))
    for k in range(n):
        P.face((centre, ring[k], ring[k + 1]))
    return P


def giant_lily(name, seed):
    """The Frog King's throne: a huge lily pad with a raised rim, standing on a thick stem."""
    P = Piece(name, seed)
    rng = P.rng
    n = 36
    centre = P.vert((0, 0, .32), LILY_DARK)
    inner, outer = [], []
    for k in range(n + 1):
        a = .3 + (2 * math.pi - .6) * k / n
        r = 3.0 * rng.uniform(.97, 1.02)
        inner.append(P.vert((math.cos(a) * r * .92, math.sin(a) * r * .92, .34), mix(LILY, LILY_EDGE, .3)))
        outer.append(P.vert((math.cos(a) * r, math.sin(a) * r, .5), LILY_EDGE))
    for k in range(n):
        P.face((centre, inner[k], inner[k + 1]))
        P.face((inner[k], outer[k], outer[k + 1], inner[k + 1]))
    base = [P.vert((math.cos(a) * 2.9, math.sin(a) * 2.9, .0), LILY_DARK) for a in [.3 + (2 * math.pi - .6) * k / n for k in range(n + 1)]]
    for k in range(n):
        P.face((outer[k], base[k], base[k + 1], outer[k + 1]))
    for v in range(6):  # raised veins
        a = .3 + (2 * math.pi - .6) * v / 5
        P.box(place(math.cos(a) * 1.4, math.sin(a) * 1.4, .36, yaw=math.degrees(a)), (2.6, .08, .05), lambda p, n: LILY_EDGE)
    return P


def plank_color(p, n):
    return mix(WOOD_DARK, WOOD, .5 + .5 * noise.noise(p * 5)) if noise.noise(p * 2.3) < .25 else mix(WOOD_ROT, WOOD_DARK, .5)


def boardwalk(name, seed, broken=False):
    """A 3 m rotten boardwalk section, 1.3 m wide, deck 35 cm above the ground: stringers on posts and loose,
    tilted planks (some missing on the broken one)."""
    P = Piece(name, seed)
    rng = P.rng
    for y in (-.55, .55):
        P.box(place(0, y, .22), (3.0, .12, .12), plank_color, jitter=.01)
    for x in (-1.35, 1.35):
        for y in (-.6, .6):
            P.box(place(x, y, -.1, pitch=rng.uniform(-4, 4), roll=rng.uniform(-4, 4)), (.14, .14, .9), plank_color, jitter=.01)
    count = 13
    for k in range(count):
        if broken and rng.random() < .3:
            continue
        x = -1.5 + (k + .5) * 3.0 / count
        sag = -.04 if broken and k > count * .6 else 0
        P.box(place(x, rng.uniform(-.04, .04), .31 + sag, yaw=rng.uniform(-3, 3), roll=rng.uniform(-2, 2), pitch=rng.uniform(-2, 2)),
              (.2, 1.3 + rng.uniform(-.1, .1), .06), plank_color, jitter=.008)
    return P


def reeds(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    for b in range(16):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0, .25)
        base = Vector((math.cos(a) * r, math.sin(a) * r, -.1))
        lean = Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(.05, .25)
        H = rng.uniform(.9, 1.6)
        side = Vector((-math.sin(a + 1.3), math.cos(a + 1.3), 0)) * .025
        pts = []
        for s in range(4):
            t = s / 3
            c = base + lean * t * t + Vector((0, 0, H * t))
            col = mix(REED, REED_TIP, t)
            pts.append((P.vert(c + side * (1 - t), col, t), P.vert(c - side * (1 - t), col, t)))
        for s in range(3):
            (a1, b1), (a2, b2) = pts[s], pts[s + 1]
            P.face((a1, b1, b2, a2))
        if b % 3 == 0:  # a cattail head
            head = base + lean + Vector((0, 0, H * .85))
            P.tube([head, head + Vector((0, 0, .16))], lambda t: .025, lambda t, a2: CATTAIL, sides=5, sway=lambda t: 1.0)
    return P


def drowned_stump(name, seed):
    P = Piece(name, seed)
    rng = P.rng
    stump = [Vector((0, 0, -.4)), Vector((.05, 0, .4)), Vector((.02, .04, 1.1))]
    P.tube(stump, lambda t: .45 - t * .1, bark, sides=9, wobble=.15, cap_end=False)
    # The jagged broken top.
    for k in range(9):
        a = 2 * math.pi * k / 9
        P.tube([Vector((math.cos(a) * .33, math.sin(a) * .33, 1.05)), Vector((math.cos(a) * .3, math.sin(a) * .3, 1.1 + rng.uniform(.05, .45)))],
               lambda t: .07 * (1 - t), bark, sides=3)
    P.lump(Vector((0, 0, 1.08)), (.3, .3, .03), lambda p, n: BARK_DARK, subdiv=1, rough=.1, seed=seed)
    for k in range(4):
        a = 2 * math.pi * k / 4 + .4
        d = Vector((math.cos(a), math.sin(a), 0))
        P.tube([d * .3 + Vector((0, 0, .2)), d * .9 + Vector((0, 0, -.15))], lambda t: .15 * (1 - t * .4), bark, sides=5, cap_end=False)
    P.lump(Vector((.1, .2, .7)), (.25, .12, .3), lambda p, n: MOSS, subdiv=1, rough=.3, seed=seed + 1)
    return P


def mud_island(name, seed):
    P = Piece(name, seed)
    P.lump(Vector((0, 0, -.05)), (1.6, 1.1, .45), lambda p, n: mix(mix(MUD_DARK, MUD, .5 + .5 * noise.noise(p * 3)), MUD_MOSS, max(0.0, n.z - .5)),
           subdiv=3, rough=.15, seed=seed, flatten=-.05)
    return P


def ripple(name, seed):
    """A flat ring for splashes and ripples; the level scales it up and fades it out."""
    P = Piece(name, seed)
    n = 32
    inner, outer = [], []
    for k in range(n):
        a = 2 * math.pi * k / n
        inner.append(P.vert((math.cos(a) * .82, math.sin(a) * .82, 0), SOUL_BRIGHT))
        outer.append(P.vert((math.cos(a), math.sin(a), 0), SOUL_BRIGHT))
    for k in range(n):
        j = (k + 1) % n
        P.face((inner[k], outer[k], outer[j], inner[j]), 1)
    return P


def exit_lantern(name, seed):
    """The light at the end of the swamp: a crooked post, a hook and a lantern with a warm flame (slot 1)."""
    P = Piece(name, seed)
    P.tube([Vector((0, 0, -.3)), Vector((.08, .03, 1.2)), Vector((.02, -.02, 2.6))], lambda t: .09 - t * .03, lambda t, a: mix(WOOD_DARK, WOOD, .3), sides=6, wobble=.1)
    P.tube([Vector((.02, -.02, 2.5)), Vector((.4, -.02, 2.75)), Vector((.8, -.02, 2.7))], lambda t: .04, lambda t, a: WOOD_DARK, sides=5)
    P.tube([Vector((.78, -.02, 2.7)), Vector((.78, -.02, 2.45))], lambda t: .01, lambda t, a: (.05, .05, .05), sides=3, sway=lambda t: .5)
    M = place(.78, -.02, 2.2)
    for x, y in ((-.1, -.1), (.1, -.1), (.1, .1), (-.1, .1)):
        P.box(M @ place(x, y, 0), (.025, .025, .34), lambda p, n: (.03, .03, .03), sway=.4)
    P.box(M @ place(z=.19), (.26, .26, .05), lambda p, n: (.03, .03, .03), sway=.4)
    P.box(M @ place(z=-.18), (.24, .24, .04), lambda p, n: (.03, .03, .03), sway=.4)
    P.lump(Vector((.78, -.02, 2.2)), (.08, .08, .12), lambda p, n: LANTERN, subdiv=1, rough=.1, seed=seed, slot=1, sway=.4)
    return P


pieces = [
    swamp_tree("SM_SwampTreeA", 1, 9.0, .6), swamp_tree("SM_SwampTreeB", 2, 7.0, 1.4), swamp_tree("SM_SwampTreeC", 3, 6.0, .9, broken=True),
    zombie_arm("SM_ZombieArm", 4), lily_pad("SM_LilyPad", 5, .55), lily_pad("SM_LilyPadSmall", 6, .32), giant_lily("SM_GiantLilyPad", 7),
    boardwalk("SM_Boardwalk", 8), boardwalk("SM_BoardwalkBroken", 9, broken=True), reeds("SM_Reeds", 10), drowned_stump("SM_DrownedStump", 11),
    mud_island("SM_MudIsland", 12), ripple("SM_Ripple", 13), exit_lantern("SM_ExitLantern", 14),
]
bpy.ops.wm.read_factory_settings(use_empty=True)
materials = (bpy.data.materials.new("Kit"), bpy.data.materials.new("Glow"))
objects = [p.build(materials) for p in pieces]
export(objects, OUT)
if PREVIEW:
    preview(objects, PREVIEW, columns=7)
print("SWAMP KIT DONE")
