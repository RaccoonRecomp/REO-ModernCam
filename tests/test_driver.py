"""Run REO_ModernCam's compiled callbacks (v2) against a fake EE RAM seeded with the real US v2.00 executable and
overlays, with a fake of the host API (HOST_API.md), and check what the mod does:

    python tests/test_driver.py

Needs build/mod.elf (run make first) and the extracted files of your own US v2.00 disc (REO_GAME_DIR, see
us_image.py). Everything happens in memory; no file is written.

The fake game frame: the frame mark calls recomp_on_play_main, then the game's pad mapper runs (the camera.padMap
mark calls recomp_on_pad_map with the player's record), then the per-character update (the camera.reload mark: a
queued guest call runs there), then the game moves and turns her. Section 26 also raises recomp_on_room_draw (the
camera.roomDraw mark: the room's draw loop) in a fake of J's Bar's restroom."""
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
from mips_sim import Machine, f2u, u2f  # noqa: E402
from us_image import elf, overlay, GAME  # noqa: E402
from settings import S, EXE  # noqa: E402

ELF = os.path.join(HERE, "..", "build", "mod.elf")

# Addresses (reo_camera_us.h)
FRAMES = 0x003243F4
CAM_EYE = 0x00324138
CAM_TGT = 0x00324144
CAM_NEAR, CAM_FAR, CAM_FOV = 0x00324174, 0x00324178, 0x00324180
PAD_PTR = 0x00276198
PAD_REC = 0x00324240
HELD_WORD = PAD_REC + 8
CHAR = 0x004A5C30
STRIDE = 0x10E0
CUT = 0x003DDDB0
WALL_PTR = 0x003C2724
CONTROL_TYPE = 0x003243EC
CULL = 0x0070A6D0
LIGHT = 0x00324098
ITEMS = 0x003C5630
COMBINE = 0x006690E0
PADBLK = 0x01F00000
SCRATCH = 0x01FFFE00  # where the fake host's scratch block is (reo_guest_scratch)
# Game code ranges: the mod must never read or write them (patches.json switches the code sites instead).
CODE = [(0x00100000, 0x00259D80), (0x00370040, 0x0037FD80), (0x00570040, 0x00708580)]

# Camera actions (reo_host.h)
AIM, SHOOT, RELOAD, SPRINT, CAPTURE, RECENTER, SWAP, SPECIAL, LENS, ALTVIEW, REVEAL = (1 << i for i in range(11))
BIND_KEYS = {AIM: "aim_button", SHOOT: "shoot_button", RELOAD: "reload_button", SPRINT: "sprint_button",
             CAPTURE: "capture_button", RECENTER: "recenter_button", SWAP: "swap_shoulder_button",
             SPECIAL: "special_button", LENS: "lens_button", ALTVIEW: "switch_view_button", REVEAL: "reveal_button"}
SCE = {"R1": 0x0800, "L2": 0x0100, "R3": 0x0004, "L3": 0x0002, "R2": 0x0200, "SQUARE": 0x8000}

fails = []
steps = []


def check(cond, msg):
    print(("  ok   " if cond else "  FAIL ") + msg)
    if not cond:
        fails.append(msg)


def default_config():
    cfg = {}
    for st in S:
        if st["kind"] == "bool":
            cfg[st["key"]] = 1 if st["default"] else 0
        elif st["kind"] == "choice":
            cfg[st["key"]] = st["options"].index(st["default"])
        else:
            cfg[st["key"]] = st["default"]
    return cfg


def hex_rgb(s):
    """ModernCam's _hex_rgb: '#rgb' or '#rrggbb' ('#' optional); None when invalid."""
    t = str(s).strip().lstrip("#")
    if len(t) == 3:
        t = "".join(c * 2 for c in t)
    if len(t) != 6:
        return None
    try:
        return int(t, 16)
    except ValueError:
        return None


class Host:
    """The fake of the REO host API (HOST_API.md)."""

    def __init__(self, m):
        self.m = m
        self.game_loaded = 1
        self.available = True
        self.requested = {"camera.freeze": 0, "camera.freeAim": 0}
        self.rate = 59940
        self.input_valid = True
        self.device = 1
        self.held = 0
        self.bound = sum(a for a, k in BIND_KEYS.items() if m.config.get(k))
        self.lx = self.ly = self.rx = self.ry = 0.0
        self.mdx = self.mdy = 0
        self.ui_open = 0
        self.hidden = 0
        self.pad_ovr = 0
        self.mouse_cap = None
        self.crosshair = None
        self.calls = []       # queued guest calls: dict(ticket, hook, function, a0, a1, a2, a3, state, v0)
        self.next_ticket = 1
        self.local_slot = 0
        self.humans = 1
        self.status = None
        self.logs = []
        self.config_writes = []
        self.in_room_draw = False  # recomp_on_room_draw runs (reo_guest_call / reo_guest_scratch work only then)
        self.now = []              # the calls of the game's code made at once: (function, argument words)
        self.scratch = []          # the scratch words, in order
        self.refused = 0
        for name in ("reo_feature_set", "reo_feature_state", "reo_image_loaded", "reo_vblank_rate_mhz",
                     "reo_config_get_rgb", "reo_config_set_float", "reo_cam_input", "reo_cam_input_hide",
                     "reo_pad_override", "reo_mouse_capture", "reo_cam_crosshair", "reo_guest_call_at",
                     "reo_guest_call_result", "reo_guest_call", "reo_guest_scratch", "reo_local_player_slot",
                     "reo_human_player_slots", "reo_cam_status", "reo_log"):
            m.handlers[name] = getattr(self, name[4:])

    def state(self, name):
        if name not in self.requested:
            return 4
        if not self.available:
            return 3
        if not self.requested[name]:
            return 0
        return 1 if self.game_loaded else 2

    def feature_set(self, m, a):
        name = m.cstr(a[0])
        if name in self.requested and self.available:
            self.requested[name] = 1 if a[1] else 0
        return self.state(name)

    def feature_state(self, m, a):
        return self.state(m.cstr(a[0]))

    def image_loaded(self, m, a):
        return self.game_loaded if m.cstr(a[0]) == "game.bin" else 0

    def vblank_rate_mhz(self, m, a):
        return self.rate

    def config_get_rgb(self, m, a):
        v = hex_rgb(m.config.get(m.cstr(a[0]), ""))
        return 0xFFFFFFFF if v is None else v

    def config_set_float(self, m, a):
        key = m.cstr(a[0])
        m.config[key] = u2f(a[1])
        self.config_writes.append((key, m.config[key]))

    def cam_input(self, m, a):
        if not self.input_valid:
            return 0
        m.set_mod_words(a[0], [1, self.device, self.held, self.bound, f2u(self.lx), f2u(self.ly), f2u(self.rx),
                               f2u(self.ry), self.mdx & 0xFFFFFFFF, self.mdy & 0xFFFFFFFF, 1, self.ui_open])
        return 1

    def cam_input_hide(self, m, a):
        self.hidden = a[0]

    def pad_override(self, m, a):
        self.pad_ovr = a[0]

    def mouse_capture(self, m, a):
        self.mouse_cap = a[0]

    def cam_crosshair(self, m, a):
        w = m.mod_words(a[0], 9)
        self.crosshair = dict(visible=w[0], style=w[1], size=w[2], gap=w[3], thickness=w[4], rgb=w[5],
                              dx=struct.unpack("<i", struct.pack("<I", w[6]))[0],
                              dy=struct.unpack("<i", struct.pack("<I", w[7]))[0], aim=u2f(w[8]))

    def guest_call_at(self, m, a):
        hook = m.cstr(a[0])
        fn, a0, a1, a2, a3, expire = m.mod_words(a[1], 6)
        if hook != "camera.reload":
            return 0
        t = self.next_ticket
        self.next_ticket += 1
        self.calls.append(dict(ticket=t, hook=hook, function=fn, a0=a0, a1=a1, a2=a2, a3=a3, expire=expire,
                               state=0, v0=0))
        return t

    def guest_call_result(self, m, a):
        for c in self.calls:
            if c["ticket"] == a[0]:
                if c["state"] == 1:
                    m.w32(a[1], c["v0"])
                return c["state"]
        return 3

    def guest_call(self, m, a):
        if not 0 <= a[2] <= 8:
            raise ValueError("reo_guest_call with %d argument words" % a[2])
        words = m.mod_words(a[1], a[2]) if a[2] else []
        if not self.in_room_draw or not any(lo <= a[0] < hi for lo, hi in CODE):
            self.refused += 1
            return 0
        self.now.append((a[0], words))
        return 1  # (the functions the mirror calls - the scissor routine, the model draw - return 1)

    def guest_scratch(self, m, a):
        if not 1 <= a[1] <= 64:
            raise ValueError("reo_guest_scratch with %d words" % a[1])
        words = m.mod_words(a[0], a[1])
        if not self.in_room_draw:
            self.refused += 1
            return 0
        self.scratch.append(words)
        return SCRATCH

    def local_player_slot(self, m, a):
        return self.local_slot

    def human_player_slots(self, m, a):
        return self.humans

    def cam_status(self, m, a):
        self.status = (a[0], a[1])

    def log(self, m, a):
        self.logs.append(m.cstr(a[0]))


def new_machine(overlays=(0, 2)):
    m = Machine(ELF)
    for va, data, _ in elf(os.path.join(GAME, "SLUS_207.65")):
        if data:
            m.ee[va: va + len(data)] = data
    for n in overlays:
        o = overlay(n)
        img = o["image"]
        m.ee[o["load"]: o["load"] + len(img)] = img
    m.config = default_config()
    return m


class Game:
    """The bits of game state the camera reads, driven by the test."""

    def __init__(self, m):
        self.m = m
        self.host = Host(m)
        self.frames = 1000
        self.px, self.py, self.pz = 1000.0, 0.0, 2000.0
        self.heading = 30.0  # degrees
        self.buttons = 0      # SCE, active-high (the game's own raw frames)
        self.padcount = 1
        self.pad_map = True   # the host calls recomp_on_pad_map
        self.held_word = 0    # what the game's pad refresh puts in the mapped record each frame
        self.mapped = []      # the held word the mapper loaded, per frame
        self.combine = self.default_combine
        m.ee_set_u32(PAD_PTR, PADBLK)
        self.write()

    def default_combine(self, a0, a1, a2):
        # The game's item-combine routine: the reload animation starts (action 0x1C, sub-state 0x40).
        self.m.ee[a0 + 0x54F] = 0x1C
        self.m.ee[a0 + 0x09] = 0x40
        return 0

    def write(self):
        m = self.m
        m.ee_set_u32(FRAMES, self.frames)
        base = CHAR
        m.ee_set_f32(base + 0x38, self.px)
        m.ee_set_f32(base + 0x3C, self.py)
        m.ee_set_f32(base + 0x40, self.pz)
        h = math.radians(self.heading)
        m.ee_set_f32(base + 0x70, math.sin(h))
        m.ee_set_f32(base + 0x78, math.cos(h))
        m.ee_set_u16(base + 0x92, int(round(self.heading % 360 * 65536 / 360)) & 0xFFFF)
        for i, cnt in ((0, self.padcount - 1), (1, self.padcount)):
            fr = PADBLK + i * 0x80
            m.ee_set_u32(fr + 0x7C, cnt)
            m.ee[fr + 2] = 6
            m.ee_set_u16(fr + 0x1C, (~self.buttons) & 0xFFFF)
            for off in (0x1E, 0x1F, 0x20, 0x21):
                m.ee[fr + off] = 0x80

    def read_heading(self):
        return self.m.ee_s16(CHAR + 0x92) * 360.0 / 65536.0

    def frame(self, n=1):
        m = self.m
        for _ in range(n):
            self.frames += 2  # 30 fps
            self.padcount += 1
            # the game refreshes the mapped record every frame (and its stick groups)
            m.ee_set_u32(HELD_WORD, self.held_word)
            m.ee_set_u32(PAD_REC + 0x20, 0x40001234)
            m.ee_set_u32(PAD_REC + 0x28, 0x40005678)
            self.write()
            m.call(m.symbols["Mcam_OnPlayMain"])
            steps.append(m.steps)
            if self.pad_map:
                m.call(m.symbols["Mcam_OnPadMap"], args=(HELD_WORD, CHAR))
            self.mapped.append(m.ee_u32(HELD_WORD))
            # the camera.reload mark at the per-character update, a0 = her record
            for c in self.host.calls:
                if c["state"] == 0 and c["a0"] == CHAR and c["function"] == COMBINE:
                    c["v0"] = self.combine(CHAR, c["a1"], c["a2"])
                    c["state"] = 1
            # she stays where the mod put her
            self.px = m.ee_f32(CHAR + 0x38)
            self.py = m.ee_f32(CHAR + 0x3C)
            self.pz = m.ee_f32(CHAR + 0x40)
            # the game integrates her heading from the heading field
            self.heading = self.read_heading()
            h = math.radians(self.heading)
            m.ee_set_f32(CHAR + 0x70, math.sin(h))
            m.ee_set_f32(CHAR + 0x78, math.cos(h))

    def eye(self):
        return tuple(self.m.ee_f32(CAM_EYE + 4 * i) for i in range(3))

    def tgt(self):
        return tuple(self.m.ee_f32(CAM_TGT + 4 * i) for i in range(3))

    def boom(self):
        e, t = self.eye(), self.tgt()
        return math.sqrt(sum((a - b) ** 2 for a, b in zip(e, t)))


def angwrap(a):
    return (a + 180.0) % 360.0 - 180.0


def expected_eye(px, py, pz, yaw, pitch, dist, sh, height, drop):
    tr, pr = math.radians(yaw), math.radians(pitch)
    fx, fz = math.sin(tr), math.cos(tr)
    rx, rz = fz, -fx
    horiz = dist * math.cos(pr)
    eye = (px + fx * horiz + rx * sh, py + height + dist * math.sin(pr), pz + fz * horiz + rz * sh)
    look = (px + rx * sh, py + height + drop, pz + rz * sh)
    return eye, look


BTN_MAP = (0x002343C0, 0x002343F0)  # the exe's button-map data table (inside its one loadable segment)


def code_touched(m):
    """Writes into the game's code ranges, and reads of them other than the button-map data table."""
    bad = [(a, w) for a, w, *_ in m.ee_writes if any(lo <= a < hi for lo, hi in CODE)]
    bad += [(a, w) for a, w in m.ee_reads if any(lo <= a < hi for lo, hi in CODE)
            and not BTN_MAP[0] <= a < BTN_MAP[1]]
    return bad


def stage_item_data(m):
    """The item definitions and combine rules of the running game (they are loaded at run time, not from the BIN
    overlays): the entries the reload uses, as the owner's probes read them (.45 type 18 holds 7; .45 magazine
    type 55; .45 rounds type 60, flag 2)."""
    defs, rules = 0x0074BB50, 0x0074C970
    for typ, flags, off, n, cap, ident in ((18, 0x00102041, 96, 2, 7, 0x190), (55, 0x00100400, 165, 2, 7, 0xD2),
                                           (60, 0x00100002, 204, 3, 255, 0x106)):
        a = defs + 24 * typ
        m.ee_set_u32(a, flags)
        m.ee[a + 4:a + 10] = struct.pack("<3h", off, n, cap)
        m.ee_set_u16(a + 0x12, ident)
    for k, (other, result, kind) in enumerate(((0xD2, 400, 12), (0x106, 400, 1))):
        m.ee[rules + 96 * 2 + 6 * k: rules + 96 * 2 + 6 * k + 6] = struct.pack("<3h", other, result, kind)


def boom_side(g, t):
    return (t[0] - g.px) * math.cos(math.radians(g.heading)) - (t[2] - g.pz) * math.sin(math.radians(g.heading))


print("== 0. settings: the exe's 89 values, ModernCam's File 1 defaults and binds")
keys = {st["key"]: st for st in S}
check(all(k in keys for k in EXE) and len(EXE) == 89, "every one of the exe's 89 values is a setting (%d settings)"
      % len(S))
exe_defaults = {"distance": 300, "shoulder": 50, "height": 130, "fov_deg": 60, "stick_speed": 140,
                "mouse_sensitivity": 0.15, "follow_strength": 0.12, "free_look_return": 1.2, "aim_sensitivity": 0.55,
                "gun_pitch_max": 45, "cam_margin": 55, "near_clip": 50, "turn_speed": 165, "reveal_time": 1.6,
                "aim_blend": 0.25, "follow_hold": 0.18, "recenter_time": 0.25, "shoulder_swap_time": 0.18,
                "crosshair_size": 3, "crosshair_gap": 3, "crosshair_thickness": 2, "cinematic_move_confirm_ms": 180,
                "seat_hold_seconds": 4, "modern_aim_deadzone": 12, "cut_match_radius": 250}
check(all(keys[k]["default"] == v for k, v in exe_defaults.items()), "numeric defaults are ModernCam's File 1 values")
binds = {"aim_button": "trig:L", "shoot_button": "trig:R", "sprint_button": "pad:0x0040",
         "swap_shoulder_button": "pad:0x0080", "reload_button": "pad:0x4000", "capture_button": "key:0x43",
         "recenter_button": "", "special_button": "", "lens_button": "", "switch_view_button": "",
         "reveal_button": ""}
check(all(keys[k]["default"] == v for k, v in binds.items()),
      "binds are ModernCam's File 1 set: Aim LT, Shoot RT, Sprint L3, Swap R3, Reload X/Square, Free cursor C")
check(keys["rotate_mode"]["default"] is False and keys["crosshair_color"]["default"] == "#f2f2f2" and
      keys["char_light_color"]["default"] == "#808080" and keys["input_device"]["default"] == "auto",
      "File 1 seed: Keep Behind off (manual), colours and device as the exe")

print("== 1. game.bin loaded, level live: freeze switched on (no code written), eye behind her")
m = new_machine()
g = Game(m)
m.log_writes = m.log_reads = True
g.frame(3)
check(g.host.requested["camera.freeze"] == 1, "camera.freeze feature requested on")
check(g.host.requested["camera.freeAim"] == 1, "camera.freeAim feature requested on (Modern Aim + Free Gun Elevation)")
e, lk = expected_eye(g.px, g.py, g.pz, g.heading + 180.0, 0.0, 300, 50, 130, 0)
ge = g.eye()
check(max(abs(a - b) for a, b in zip(ge, e)) < 0.5, "eye behind her at ModernCam's 300/50/130: got %s want %s" % (
    tuple(round(v, 1) for v in ge), tuple(round(v, 1) for v in e)))
check(max(abs(a - b) for a, b in zip(g.tgt(), lk)) < 0.5, "look-at on her shoulder line")
check(abs(m.ee_f32(CAM_FOV) - 60.0) < 1e-4, "FOV written (60)")
check(abs(m.ee_f32(CAM_NEAR) - 50.0) < 1e-4, "near value written (50)")
check(g.host.status == (2, 0), "status: driving (%s)" % (g.host.status,))
check(g.host.crosshair is not None and not g.host.crosshair["visible"], "crosshair hidden while not aiming")

print("== 2. right stick right for 1 s turns the camera clockwise at 140 deg/s; the game's right stick is centred")
g.host.rx = 1.0
g.frame(30)
g.host.rx = 0.0
e1, t1 = g.eye(), g.tgt()
boom_yaw = math.degrees(math.atan2(e1[0] - t1[0], e1[2] - t1[2]))
turned = angwrap(boom_yaw - (g.heading + 180.0))
check(-150 < turned < -125, "turned %.1f deg (expected about -140)" % turned)
check(m.ee_u32(PAD_REC + 0x28) == 0xC0000000 and m.ee_u32(PAD_REC + 0x2C) == 0,
      "the mapped record's right stick centred at the pad-map mark")
check(g.host.pad_ovr & 2, "the virtual pad's right stick centred (own look stick)")

print("== 3. right stick up tilts the view up (pitch clamps at -35)")
g.host.ry = 1.0
g.frame(60)
g.host.ry = 0.0
e2, t2 = g.eye(), g.tgt()
check(e2[1] < t2[1], "eye below the look-at point (looking up): eye y %.1f, look y %.1f" % (e2[1], t2[1]))
pitch = math.degrees(math.asin((e2[1] - t2[1]) / 300.0))
check(abs(pitch - (-35.0)) < 0.6, "pitch at the lower clamp: %.1f" % pitch)

print("== 4. aim (left trigger): gun elevation follows the camera, R1 held for the game, crosshair, she turns")
g.host.held = AIM
g.frame(40)
ang = m.ee_s16(CHAR + 0xBC8) * 360.0 / 65536.0
check(ang > 0, "gun raised while the view looks up: %.1f deg" % ang)
check(g.mapped[-1] & 0x400, "R1 held for the game in the mapper's word at the pad-map mark (0x%x)" % g.mapped[-1])
check(g.host.hidden & AIM, "the aim bind is hidden from the game while held")
xh = g.host.crosshair
check(xh["visible"] and xh["style"] == 0 and xh["size"] == 3 and xh["rgb"] == 0xF2F2F2,
      "crosshair shown while aiming: dot, size 3, #f2f2f2 (%s)" % xh)
e3, t3 = g.eye(), g.tgt()
cam_yaw = math.degrees(math.atan2(e3[0] - t3[0], e3[2] - t3[2]))
check(abs(angwrap((cam_yaw - 180.0) - g.heading)) < 13.0, "she faces where the camera looks: view %.1f, heading %.1f"
      % (angwrap(cam_yaw - 180.0), g.heading))
g.host.held = 0
g.frame(10)
check(m.ee_s16(CHAR + 0xBC8) == 0, "gun reset when aim is released")
check(not g.host.crosshair["visible"], "crosshair hidden after aiming")
check(not (g.mapped[-1] & 0x400), "R1 no longer pressed")

print("== 5. aim-walk: left stick moves her while aiming")
x0, z0 = g.px, g.pz
g.host.held = AIM
g.host.ly = 1.0
g.frame(30)
moved = math.hypot(g.px - x0, g.pz - z0)
check(35 < moved < 55, "moved %.1f units in 1 s at 45 u/s" % moved)
check(m.ee_u32(PAD_REC + 0x20) == 0xC0000000, "the game's left stick centred while aim-walking")
g.host.ly = 0.0
g.host.held = 0
g.frame(20)

print("== 6. wall between her and the eye pulls the camera in")
m.config["rotate_mode"] = 1  # keep behind, so the boom is predictable
g.frame(90)
e4, t4 = g.eye(), g.tgt()
boom = math.hypot(e4[0] - t4[0], e4[2] - t4[2])
mx, mz = (e4[0] + t4[0]) / 2, (e4[2] + t4[2]) / 2
dx, dz = (e4[0] - t4[0]) / boom, (e4[2] - t4[2]) / boom
px_, pz_ = -dz * 400, dx * 400
WALLS = 0x01E00000
tri = [(mx - px_, -100.0, mz - pz_), (mx + px_, -100.0, mz + pz_), (mx + px_, 400.0, mz + pz_)]
m.ee_set_u32(WALLS, 0x002D0000)
for i, (x, y, z) in enumerate(tri):
    m.ee_set_f32(WALLS + 4 + i * 12, x)
    m.ee_set_f32(WALLS + 8 + i * 12, y)
    m.ee_set_f32(WALLS + 12 + i * 12, z)
m.ee_set_f32(WALLS + 0x28, dx)
m.ee_set_f32(WALLS + 0x2C, 0.0)
m.ee_set_f32(WALLS + 0x30, dz)
m.ee_set_u32(WALLS + 0x38, 0)  # end of list
m.ee_set_u32(WALL_PTR, WALLS)
g.frame(3)
e5, t5 = g.eye(), g.tgt()
boom5 = math.hypot(e5[0] - t5[0], e5[2] - t5[2])
check(boom5 < boom * 0.5, "boom pulled in from %.0f to %.0f" % (boom, boom5))
m.ee_set_u32(WALL_PTR, 0)
g.frame(60)
e6, t6 = g.eye(), g.tgt()
check(math.hypot(e6[0] - t6[0], e6[2] - t6[2]) < boom5 + 0.5, "wall gone: the boom does not pull back by itself")
m.ee_set_u32(CHAR + 0xF80, 0x1)  # the player moves her (the game's forward action)
g.frame(60)
m.ee_set_u32(CHAR + 0xF80, 0)
e6, t6 = g.eye(), g.tgt()
b6 = math.hypot(e6[0] - t6[0], e6[2] - t6[2])
check(boom * 0.9 < b6 < boom + 0.5, "the player moves her: the boom returns to its distance %.0f, never further" % b6)

print("== 7. cut matching forces the nearest authored cut")
m.ee[CUT + 1] = 2
m.ee[CUT + 3] = 0
e7 = g.eye()
far = (int(e7[0] + 2000), int(e7[1]), int(e7[2] + 2000))
near = (int(e7[0] + 30), int(e7[1]), int(e7[2] - 20))
for i, (x, y, z) in enumerate((far, near)):
    rec = CUT + 0x34 + i * 0x198 + 0xA0
    for j, v in enumerate((x, y, z)):
        m.ee[rec + 4 * j: rec + 4 * j + 4] = struct.pack("<i", v)
g.frame(20)
check(m.ee[CUT + 5] == 1 and m.ee[CUT + 6] == 1, "cut 1 forced (idx %d, flag %d)" % (m.ee[CUT + 5], m.ee[CUT + 6]))

print("== 8. the engine moves the camera (door): taken back on the second frame")
m.ee_set_f32(CAM_EYE, 99999.0)
g.frame(2)
check(abs(g.eye()[0] - 99999.0) > 1000, "camera driven again after a take-over")

print("== 9. draw distance never hides nearby characters; it stops at 1 and leaves the far plane alone")
# Probed on the game: x4 / x50 of the far plane showed nothing more in J's Bar, Hellfire's Apple Inn square and
# Decisions, Decisions' exterior shots, so v2 no longer scales it; above 1 the exe only enlarged the hide radii.
dd = keys["draw_distance"]
check(dd["min"] == 0.1 and dd["max"] == 1 and dd["default"] == 1.0,
      "Draw Distance ranges 0.1 to 1, default 1 (%s..%s)" % (dd["min"], dd["max"]))
m.config["enabled"] = 0
g.frame(1)
m.ee_set_f32(CAM_FAR, 50000.0)  # the game's far plane (the camera init's 50,000)
m.config["enabled"] = 1
m.config["draw_distance"] = 4.0  # a value saved above the range (v1 or an earlier v2)
g.frame(4)
tbl = [m.ee_f32(CULL + 4 * i) for i in range(4)]
check(tbl == [20.0, 80.0, 180.0, 300.0] and abs(m.ee_f32(CAM_FAR) - 50000.0) < 1.0,
      "x4 saved: acts as 1, the hide radii stay 20/80/180/300 and the far plane 50,000 (%s, %.0f)"
      % (tbl, m.ee_f32(CAM_FAR)))
m.config["draw_distance"] = 0.5
g.frame(2)
tbl = [m.ee_f32(CULL + 4 * i) for i in range(4)]
check(tbl == [10.0, 40.0, 90.0, 150.0], "x0.5: the hide radii halve as in the exe (%s)" % tbl)
check(abs(m.ee_f32(CAM_FAR) - 50000.0) < 1.0, "x0.5: far plane untouched (%.0f)" % m.ee_f32(CAM_FAR))
m.config["far_clip"] = 30000
g.frame(2)
check(abs(m.ee_f32(CAM_FAR) - 30000.0) < 1.0, "Far Clip 30,000 is written (%.0f)" % m.ee_f32(CAM_FAR))
m.config["far_clip"] = 0
m.ee_set_f32(CAM_FAR, 50000.0)  # the next camera init puts the game's value back
m.config["draw_distance"] = 1.0
g.frame(2)
check([m.ee_f32(CULL + 4 * i) for i in range(4)] == [20.0, 80.0, 180.0, 300.0], "x1: radii restored")

print("== 10. character light colour")
m.config["char_light"] = 1
m.config["char_light_color"] = "#ff8000"
g.frame(2)
check(m.ee_u32(LIGHT) & 0xFFFFFF == 0x0080FF and m.ee[0x003240BC] == 2,
      "light RGB #ff8000 written with mode 2 (0x%08x)" % m.ee_u32(LIGHT))
m.config["char_light"] = 0
g.frame(2)

print("== 11. Modern Camera off: everything handed back")
m.ee_set_f32(CAM_NEAR, 500.0)
m.config["enabled"] = 0
g.frame(2)
check(g.host.requested["camera.freeze"] == 0 and g.host.requested["camera.freeAim"] == 0,
      "camera.freeze and camera.freeAim switched off")
check(m.ee[CUT + 6] == 0, "forced cut released")
check(g.host.status == (0, 0), "status: off")
check(g.host.hidden == 0 and g.host.pad_ovr == 0 and not g.host.crosshair["visible"],
      "no hidden binds, no pad override, no crosshair")
bad = code_touched(m)
print("  (code accesses: %s)" % sorted(set("%08X" % a for a, w in bad))[:20])
check(not bad, "no read or write of game code in all of the above (%d)" % len(bad))
m.config["enabled"] = 1

print("== 12. game.bin not loaded (another overlay): nothing is written, features off")
m2 = new_machine(overlays=(0, 3))
g2 = Game(m2)
g2.host.game_loaded = 0
m2.log_writes = m2.log_reads = True
g2.frame(5)
check(not [w for w in m2.ee_writes if 0x00300000 <= w[0] < 0x00800000], "no game writes (%d)" % len(m2.ee_writes))
check(g2.host.requested["camera.freeze"] == 0, "freeze not requested")
check(g2.host.status == (1, 0), "status: waiting for a level")

print("== 13. binds: sprint toggle (L3), shoot (right trigger), swap shoulder (R3) saved, special action")
m = new_machine()
g = Game(m)
m.ee[CONTROL_TYPE] = 0  # type A: square=USE, cross=FIRE, circle=DASH
g.frame(3)
g.host.ly = 1.0  # walking
g.host.held = SPRINT
g.frame(1)
check(g.mapped[-1] & 0x20, "sprint on: dash (circle, source bit 0x20) pressed for the game (0x%x)" % g.mapped[-1])
check(g.host.hidden & SPRINT, "the sprint bind is hidden from the game while held")
g.host.held = 0
g.held_word = 0x20  # the player also holds the game's own dash button
g.frame(1)
check(g.mapped[-1] & 0x20, "toggle stays on after L3 is let go")
g.host.ly = 0.0
g.frame(2)
check(not (g.mapped[-1] & 0x20), "toggle drops when she stops, and the game's own dash button is taken away")
g.held_word = 0
g.host.held = SHOOT
g.frame(1)
check(g.mapped[-1] & 0x10, "shoot: fire (cross, source bit 0x10) pressed (0x%x)" % g.mapped[-1])
g.host.held = 0
g.frame(1)
m.config["special_button"] = "pad:0x0200"
g.host.bound |= SPECIAL
g.host.held = SPECIAL
g.frame(1)
check(g.mapped[-1] & 0x20, "special action: the dash action's button pressed while held (0x%x)" % g.mapped[-1])
check(g.host.hidden & SPECIAL, "the special bind is hidden from the game")
g.host.held = 0
g.frame(1)
t_a = g.tgt()
g.host.held = SWAP
g.frame(1)
g.host.held = 0
g.frame(30)
t_b = g.tgt()
check(boom_side(g, t_a) * boom_side(g, t_b) < 0, "R3 moved the camera to the other shoulder (%.0f -> %.0f)"
      % (boom_side(g, t_a), boom_side(g, t_b)))
check(("shoulder", -50.0) in g.host.config_writes, "the new side is saved: shoulder = -50 (%s)" % g.host.config_writes)

print("== 14. level unloads (her record empties): camera handed back; next level: freeze again")
g.px = g.py = g.pz = 0.0
g.frame(2)
check(g.host.requested["camera.freeze"] == 0, "freeze off while no level is live")
g.px, g.py, g.pz = 500.0, 0.0, 500.0
g.frame(3)
check(g.host.requested["camera.freeze"] == 1, "and on again when the next level is live")

print("== 15. cinematic yield: repeated engine take-overs hand the camera back until she moves")
m.config["cinematic_yield"] = 1
g.frame(5)
for _ in range(8):
    m.ee_set_f32(CAM_EYE, 77777.0)
    g.frame(1)
check(g.host.requested["camera.freeze"] == 0, "camera handed back during the cinematic")
check(g.host.status[0] == 3, "status: handed back to a cutscene")
g.frame(20)  # stick neutral
g.host.ly = 1.0
for _ in range(10):
    g.frame(1)
    g.pz += 5.0  # the game moves her
check(g.host.requested["camera.freeze"] == 1, "camera taken again once she walks")
g.host.ly = 0.0
m.config["cinematic_yield"] = 0

print("== 16. keyboard & mouse: mouse look, free cursor, the right stick is left to the game")
m = new_machine()
g = Game(m)
g.host.device = 2
g.frame(3)
e0, t0 = g.eye(), g.tgt()
yaw0 = math.degrees(math.atan2(e0[0] - t0[0], e0[2] - t0[2]))
g.host.mdx = 100
g.frame(1)
g.host.mdx = 0
e1, t1 = g.eye(), g.tgt()
yaw1 = math.degrees(math.atan2(e1[0] - t1[0], e1[2] - t1[2]))
check(abs(angwrap(yaw1 - yaw0) - (-100 * 0.15 * 0.4)) < 0.2, "100 counts right turn the camera %.2f deg (want -6.0)"
      % angwrap(yaw1 - yaw0))
check(g.host.mouse_cap == 1, "mouse captured on Keyboard & Mouse")
check(not (g.host.pad_ovr & 2), "right stick not centred on Keyboard & Mouse (as the exe)")
g.host.held = CAPTURE
g.frame(1)
g.host.held = 0
g.host.mdx = 100
g.frame(1)
g.host.mdx = 0
e2, t2 = g.eye(), g.tgt()
yaw2 = math.degrees(math.atan2(e2[0] - t2[0], e2[2] - t2[2]))
check(g.host.mouse_cap == 0 and abs(angwrap(yaw2 - yaw1)) < 0.01, "free cursor: capture off, mouse is not look")

print("== 17. reload: the game's item-combine routine is called at the camera.reload mark")
m = new_machine()
g = Game(m)
gun, ammo, mag = 81, 93, 94
for slot, typ, count, ident in ((gun, 18, 2, 0x190), (ammo, 60, 14, 0x106), (mag, 55, 7, 0xD2)):
    a = ITEMS + 60 * slot
    m.ee[a] = 1
    m.ee_set_u16(a + 6, ident)
    m.ee_set_u32(a + 0x24, count)
    m.ee_set_u16(a + 0x28, 1)
    m.ee[a + 0x2B] = typ
m.ee[CHAR + 0xC7C] = gun
stage_item_data(m)
g.frame(3)
g.host.held = RELOAD
g.frame(1)
g.host.held = 0
g.frame(2)
c = g.host.calls[-1] if g.host.calls else {}
check(c.get("function") == COMBINE and c.get("a0") == CHAR and c.get("a1") == gun and c.get("a2") == ammo,
      "combine(a0 = her record, weapon 81, loose rounds 93 first) queued at camera.reload (%s)" % c)
check(c.get("state") == 1 and m.ee[CHAR + 0x09] == 0x40, "the call ran at the mark: the combine animation runs")
m.config["debug_log"] = 1
g.frame(40)  # the combine animation (sub-state 0x40) runs
m.ee[CHAR + 0x09] = 0
m.ee[CHAR + 0x54F] = 0
g.frame(20)
m.ee[ITEMS + 60 * ammo + 0x24: ITEMS + 60 * ammo + 0x28] = bytes(4)  # no loose rounds left
g.host.held = RELOAD
g.frame(1)
g.host.held = 0
g.frame(2)
c = g.host.calls[-1]
check(c["a2"] == mag, "without loose rounds the magazine is used (slot %d)" % c["a2"])
check(any("combine started" in s for s in g.host.logs), "event log: combine started")
m.ee[CHAR + 0x09] = 0
g.frame(20)
m.ee_set_u32(ITEMS + 60 * gun + 0x24, 7)
n = len(g.host.calls)
g.host.held = RELOAD
g.frame(1)
g.host.held = 0
g.frame(2)
check(len(g.host.calls) == n, "magazine full: no call")
# reload while aiming: she lowers the gun first, the call waits for the aim stance to end
m.ee_set_u32(ITEMS + 60 * gun + 0x24, 3)
g.host.held = AIM
m.ee[CHAR + 0x08] = 2  # the game's aim stance
g.frame(20)
g.host.held = AIM | RELOAD
g.frame(1)
check(not (g.mapped[-1] & 0x400), "reload drops the aim: R1 released for the game")
check(m.ee_u32(PAD_REC + 0x20) == 0xC0000000, "and the left stick is centred while the reload waits")
check(len(g.host.calls) == n, "no call while she still aims")
m.ee[CHAR + 0x08] = 0  # the stance ends
g.frame(2)
check(len(g.host.calls) == n + 1, "the call is made once the aim stance has ended")
g.host.held = 0
m.config["reload_animated"] = 0
m.ee[CHAR + 0x09] = 0
g.frame(40)
g.host.held = RELOAD
g.frame(1)
g.host.held = 0
g.frame(2)
check(len(g.host.calls) == n + 1, "Animated Reload off: no reload (as the exe)")

print("== 18. Aim and Alt rigs: a value applies only while its custom switch is on (the exe's per-key inheritance)")
m = new_machine()
g = Game(m)
m.config["aim_distance"] = 495
g.frame(3)
g.host.held = AIM
g.frame(60)
check(abs(g.boom() - 300) < 2, "aim_distance 495 without its switch: aiming keeps 300 (%.0f)" % g.boom())
m.config["aim_distance_set"] = 1
g.frame(60)
check(abs(g.boom() - 495) < 3, "Aim: Custom Distance on: 495 while aiming (%.0f)" % g.boom())
g.host.held = 0
g.frame(60)
check(abs(g.boom() - 300) < 3, "back to 300 after aiming (%.0f)" % g.boom())
m.config["alt_distance_set"] = 1
m.config["alt_distance"] = 425
m.config["switch_view_button"] = "key:0x56"
g.host.bound |= ALTVIEW
g.host.held = ALTVIEW
g.frame(1)
g.host.held = 0
g.frame(2)
check(abs(g.boom() - 425) < 2, "Alt View: 425 (%.0f)" % g.boom())
g.host.held = AIM
g.frame(60)
check(abs(g.boom() - 495) < 3, "aiming in Alt View uses the Aim rig's distance (%.0f)" % g.boom())
g.host.held = 0
g.frame(30)
m.config["aim_look_drop_set"] = 1
m.config["aim_look_drop"] = 40
m.config["aim_distance_set"] = 0
g.host.held = AIM
g.frame(60)
e, t = g.eye(), g.tgt()
check(abs(t[1] - (g.py + 130)) < 1.0, "look drop fades out while aiming, as the exe (look y %.1f)" % t[1])
g.host.held = 0

print("== 19. camera side (follow_dir), yaw offset, recenter time")
m = new_machine()
g = Game(m)
m.config["rotate_mode"] = 1
m.config["follow_dir"] = 1  # In front of her
g.frame(3)
g.host.ly = 1.0
for _ in range(90):
    g.frame(1)
    g.pz += 6.0 * math.cos(math.radians(g.heading))
    g.px += 6.0 * math.sin(math.radians(g.heading))
g.host.ly = 0.0
e, t = g.eye(), g.tgt()
front = (e[0] - g.px) * math.sin(math.radians(g.heading)) + (e[2] - g.pz) * math.cos(math.radians(g.heading))
check(front > 100, "In front of her: the eye swings to the side she faces (%.0f along her facing)" % front)
m.config["follow_dir"] = 0
m.config["yaw_offset"] = 90
g.host.ly = 1.0
for _ in range(120):
    g.frame(1)
    g.pz += 6.0 * math.cos(math.radians(g.heading))
    g.px += 6.0 * math.sin(math.radians(g.heading))
g.host.ly = 0.0
e, t = g.eye(), g.tgt()
yaw = math.degrees(math.atan2(e[0] - t[0], e[2] - t[2]))
check(abs(angwrap(yaw - (g.heading + 180 + 90))) < 20, "yaw offset 90: the camera sits 90 deg round (%.0f vs %.0f)"
      % (angwrap(yaw), angwrap(g.heading + 270)))
m.config["yaw_offset"] = 0
m.config["rotate_mode"] = 0
m.config["recenter_button"] = "pad:0x0020"
g.host.bound |= RECENTER
g.host.rx = 1.0
g.frame(20)
g.host.rx = 0.0
g.host.held = RECENTER
g.frame(1)
g.host.held = 0
g.frame(8)  # recenter_time 0.25 s
e, t = g.eye(), g.tgt()
yaw = math.degrees(math.atan2(e[0] - t[0], e[2] - t[2]))
check(abs(angwrap(yaw - (g.heading + 180))) < 2, "Recenter brings the camera back behind her within 0.25 s (%.1f)"
      % angwrap(yaw - (g.heading + 180)))

print("== 20. reveal camera: blends to the room's own cut and back")
m = new_machine()
g = Game(m)
m.ee[CUT + 1] = 1
m.ee[CUT + 3] = 0
rec = CUT + 0x34
for j, v in enumerate((1111, 222, 3333)):
    m.ee[rec + 0xA0 + 4 * j: rec + 0xA4 + 4 * j] = struct.pack("<i", v)
for j, v in enumerate((1000, 100, 2000)):
    m.ee[rec + 0xB8 + 4 * j: rec + 0xBC + 4 * j] = struct.pack("<i", v)
m.config["reveal_button"] = "key:0x52"
m.config["cut_match"] = 0
g.host.bound |= REVEAL
g.frame(3)
g.host.held = REVEAL
g.frame(1)
g.host.held = 0
g.frame(50)  # 1.6 s default = 48 game frames
check(max(abs(a - b) for a, b in zip(g.eye(), (1111, 222, 3333))) < 1, "revealed: the eye is the cut's (%s)" %
      (tuple(round(v) for v in g.eye()),))
g.host.held = REVEAL
g.frame(1)
g.host.held = 0
g.frame(50)
e, lk = expected_eye(g.px, g.py, g.pz, g.heading + 180.0, 0.0, 300, 50, 130, 0)
check(max(abs(a - b) for a, b in zip(g.eye(), e)) < 1, "and back behind her")

print("== 21. watch next player: offline nobody else to watch; online the rig follows the other player")
m = new_machine()
g = Game(m)
m.config["lens_button"] = "key:0x4C"
m.config["debug_log"] = 1
g.host.bound |= LENS
other = CHAR + STRIDE
m.ee_set_f32(other + 0x38, 3000.0)
m.ee_set_f32(other + 0x40, 3000.0)
m.ee_set_f32(other + 0x78, 1.0)
g.frame(3)
g.host.held = LENS
g.frame(1)
g.host.held = 0
g.frame(2)
check(abs(g.tgt()[0] - 3000) > 500 and any("needs online play" in s for s in g.host.logs),
      "offline: the camera stays on her, logged as needing online play")
g.host.humans = 0b11
g.host.held = LENS
g.frame(1)
g.host.held = 0
g.frame(5)
check(abs(g.tgt()[2] - 3000) < 200 and g.host.status[1] & 8, "online: the rig follows player 2 (look %s)" %
      (tuple(round(v) for v in g.tgt()),))

print("== 22. aim zoom speeds: 0 falls back to aim_blend (the exe's default key)")
m = new_machine()
g = Game(m)
m.config["aim_blend_in"] = 0
m.config["aim_distance_set"] = 1
m.config["aim_distance"] = 500
g.frame(3)
g.host.held = AIM
g.frame(1)
a1 = g.boom()
m2 = new_machine()
g2 = Game(m2)
m2.config["aim_blend_in"] = 0.25
m2.config["aim_distance_set"] = 1
m2.config["aim_distance"] = 500
g2.frame(3)
g2.host.held = AIM
g2.frame(1)
check(abs(a1 - g2.boom()) < 0.01 and a1 > 300, "zoom-in speed 0 = aim_blend 0.25 (%.2f vs %.2f)" % (a1, g2.boom()))

print("== 23. the game code has no camera sites: the camera does not drive")
m = new_machine()
g = Game(m)
g.host.available = False
m.log_writes = True
g.frame(5)
check(g.host.status[0] == 4, "status: unavailable (%s)" % (g.host.status,))
check(not [w for w in m.ee_writes if w[0] in range(CAM_EYE, CAM_EYE + 24)], "the eye is not written")

print("== 24. no pad-map event from the host: v1's per-frame write as a fallback")
m = new_machine()
g = Game(m)
g.pad_map = False
m.config["debug_log"] = 1
g.frame(3)
g.host.held = AIM
g.frame(40)
check(m.ee_u32(HELD_WORD) & 0x400 and any("no pad-map event" in s for s in g.host.logs),
      "R1 written once per frame, and logged")

print("== 25. no camera input service: look and move from the game's pad frames")
m = new_machine()
g = Game(m)
g.host.input_valid = False
g.frame(3)
e0 = g.eye()
for i in (0, 1):
    m.ee[PADBLK + i * 0x80 + 0x1E] = 0xFF
g.write = (lambda orig: (lambda: (orig(), [m.ee.__setitem__(PADBLK + i * 0x80 + 0x1E, 0xFF) for i in (0, 1)])))(
    g.write)
g.frame(10)
check(max(abs(a - b) for a, b in zip(g.eye(), e0)) > 10, "the right stick of the game's pad turns the camera")

print("== 27. the game's aim lock: the camera stays on the target, on its side, and never pulls back")
# The game's auto-aim (reo_camera_us.h, REO_CHAR_AIM_TARGET): in the aim stance (2) with the aim action held, her
# record's +0x24 points at the enemy record the game chose (0x0042EB00 + index * 0xD90).
ENEMY = 0x0042EB00 + 44 * 0xD90


def stage_lock(m, x, z, stance=2, held=0x400):
    m.ee[ENEMY] = 1
    m.ee_set_u32(ENEMY + 0x34, 0x00460000)
    m.ee_set_u32(ENEMY + 0xC94, 0)
    m.ee_set_f32(ENEMY + 0x38, x)
    m.ee_set_f32(ENEMY + 0x3C, 0.0)
    m.ee_set_f32(ENEMY + 0x40, z)
    m.ee[CHAR + 0x08] = stance
    m.ee_set_u32(CHAR + 0xF80, held)
    m.ee_set_u32(CHAR + 0x24, ENEMY)


def view_yaw(g):
    e, t = g.eye(), g.tgt()
    return math.degrees(math.atan2(t[0] - e[0], t[2] - e[2]))


def to_target(g, x, z):
    return math.degrees(math.atan2(x - g.px, z - g.pz))


def wall_across_boom(m, g):
    e, t = g.eye(), g.tgt()
    boom = math.hypot(e[0] - t[0], e[2] - t[2])
    mx, mz = (e[0] + t[0]) / 2, (e[2] + t[2]) / 2
    dx, dz = (e[0] - t[0]) / boom, (e[2] - t[2]) / boom
    px_, pz_ = -dz * 400, dx * 400
    walls = 0x01E00000
    tri = [(mx - px_, -100.0, mz - pz_), (mx + px_, -100.0, mz + pz_), (mx + px_, 400.0, mz + pz_)]
    m.ee_set_u32(walls, 0x002D0000)
    for i, (x, y, z) in enumerate(tri):
        m.ee_set_f32(walls + 4 + i * 12, x)
        m.ee_set_f32(walls + 8 + i * 12, y)
        m.ee_set_f32(walls + 12 + i * 12, z)
    m.ee_set_f32(walls + 0x28, dx)
    m.ee_set_f32(walls + 0x2C, 0.0)
    m.ee_set_f32(walls + 0x30, dz)
    m.ee_set_u32(walls + 0x38, 0)
    m.ee_set_u32(WALL_PTR, walls)


m = new_machine()
g = Game(m)
m.log_writes = m.log_reads = True
m.config["rotate_mode"] = 1  # Keep Behind Character, as the owner plays
m.config["debug_log"] = 1
g.frame(3)
# the enemy 400 units off her right side: the camera behind her looks 90 deg away from it
ex = g.px + 400.0 * math.sin(math.radians(g.heading - 90.0))
ez = g.pz + 400.0 * math.cos(math.radians(g.heading - 90.0))
side0 = boom_side(g, g.tgt())
g.host.held = AIM
stage_lock(m, ex, ez)
g.frame(20)
check(abs(angwrap(view_yaw(g) - to_target(g, ex, ez))) < 2.0,
      "the camera turns onto the target (view %.1f, target %.1f)" % (view_yaw(g), to_target(g, ex, ez)))
check(abs(angwrap(g.heading - to_target(g, ex, ez))) <= 12.5,
      "Modern Aim turns her to the target, not away from it (facing %.1f, target %.1f)"
      % (g.heading, to_target(g, ex, ez)))
check(any("aim lock: on" in s for s in g.host.logs), "logged: aim lock on")
# the target walks round her: the camera stays on it
for k in range(30):
    a = math.radians(g.heading - 90.0 + 3.0 * (k + 1))
    ex, ez = g.px + 400.0 * math.sin(a), g.pz + 400.0 * math.cos(a)
    m.ee_set_f32(ENEMY + 0x38, ex)
    m.ee_set_f32(ENEMY + 0x40, ez)
    g.frame(1)
g.frame(10)
check(abs(angwrap(view_yaw(g) - to_target(g, ex, ez))) < 2.0,
      "the target moved round her: the camera stays on it (view %.1f, target %.1f)"
      % (view_yaw(g), to_target(g, ex, ez)))
# the right stick does not turn the camera off the target while the lock holds
g.host.rx = 1.0
g.frame(15)
g.host.rx = 0.0
check(abs(angwrap(view_yaw(g) - to_target(g, ex, ez))) < 2.0, "the right stick does not take it off the target")
# Swap Shoulder while locked: the side stays (and is not saved)
writes = len(g.host.config_writes)
g.host.held = AIM | SWAP
g.frame(1)
g.host.held = AIM
g.frame(20)
check(boom_side(g, g.tgt()) * side0 > 0 and len(g.host.config_writes) == writes,
      "Swap Shoulder while locked: the camera stays on its side (%.0f -> %.0f), nothing saved"
      % (side0, boom_side(g, g.tgt())))
check(any("shoulder: kept" in s for s in g.host.logs), "logged: shoulder kept")
# never pulls back: a wall pulls the boom in; when it goes the boom stays in until the lock ends
b_lock = g.boom()
wall_across_boom(m, g)
g.frame(3)
b_in = g.boom()
m.ee_set_u32(WALL_PTR, 0)
g.frame(60)
b_after = g.boom()
check(b_in < b_lock * 0.6 and b_after <= b_in + 0.5,
      "wall: pulled in from %.0f to %.0f; wall gone: it stays %.0f while locked" % (b_lock, b_in, b_after))
# a hit while aiming (stance 3, aim held) keeps the lock
m.ee[CHAR + 0x08] = 3
g.frame(20)
check(abs(angwrap(view_yaw(g) - to_target(g, ex, ez))) < 2.0 and g.boom() <= b_in + 0.5,
      "hit while aiming (stance 3): still on the target, still %.0f" % g.boom())
# the aim is let go: the lock ends after its short grace; the camera turns back behind her, on its side, at its length
# (no pull-back); the boom returns to its distance when the player moves her
v_end = view_yaw(g)
m.ee[CHAR + 0x08] = 1
m.ee_set_u32(CHAR + 0xF80, 0)
g.host.held = 0
g.frame(90)
check(any("aim lock: released" in s for s in g.host.logs) and abs(angwrap(view_yaw(g) - g.heading)) < 1.0 and
      g.boom() <= b_in + 0.5 and boom_side(g, g.tgt()) * side0 > 0,
      "aim let go: lock released, the camera back behind her (view %.1f -> %.1f, facing %.1f), same side, no "
      "pull-back (%.0f)" % (v_end, view_yaw(g), g.heading, g.boom()))
m.ee_set_u32(CHAR + 0xF80, 0x1)
g.frame(60)
m.ee_set_u32(CHAR + 0xF80, 0)
check(g.boom() > b_lock * 0.9, "the player moves her: the boom returns to its distance (%.0f)" % g.boom())
v0 = view_yaw(g)
g.host.rx = 1.0
g.frame(10)
g.host.rx = 0.0
check(abs(angwrap(view_yaw(g) - v0)) > 20.0, "after the lock the right stick turns the camera again")
# Aim: Custom Distance longer than Main: the lock does not pull the camera back to it
m.config["aim_distance_set"] = 1
m.config["aim_distance"] = 495
g.host.held = AIM
stage_lock(m, ex, ez)
g.frame(1)
b1 = g.boom()
g.frame(40)
check(g.boom() <= b1 + 0.5 and g.boom() < 480,
      "locked while a longer aim distance blends in: the boom stays %.0f (not %d)" % (g.boom(), 495))
# an enemy the game can no longer target (+0xC94 bit 0) ends the lock; the camera does not pull back to the longer
# aim distance by itself
m.ee_set_u32(ENEMY + 0xC94, 1)
b_rel = g.boom()
g.frame(10)
check(any(s.endswith("aim lock: released") for s in g.host.logs[-3:]) and g.boom() <= b_rel + 0.5,
      "the target can no longer be targeted: lock released, the boom stays %.0f (no pull-back to %d)"
      % (g.boom(), 495))
m.ee_set_u32(CHAR + 0xF80, 0x1)
g.frame(60)
m.ee_set_u32(CHAR + 0xF80, 0)
check(g.boom() > 480, "the player moves her: the aim distance applies (%.0f)" % g.boom())
# no target (+0x24 = 0): Modern Aim as before, the camera stays where it is
m.config["aim_distance_set"] = 0
m.ee_set_u32(CHAR + 0x24, 0)
g.frame(10)
v1 = view_yaw(g)
g.frame(20)
check(abs(angwrap(view_yaw(g) - v1)) < 0.5, "aiming without a target: the camera does not move by itself")
check(not code_touched(m), "no game code read or written")

print("== 28. the camera stays on her: no swing by itself, the side stays, no pull-back; Lock On Enemy off")
m = new_machine()
g = Game(m)
m.log_writes = m.log_reads = True
m.config["rotate_mode"] = 1  # Keep Behind Character, as the owner plays
m.config["debug_log"] = 1
g.frame(30)
v0, side0, b0 = view_yaw(g), boom_side(g, g.tgt()), g.boom()
# a knockback: the game pushes her back 120 units over 12 frames (no input)
h = math.radians(g.heading)
for k in range(12):
    g.px -= 10.0 * math.sin(h)
    g.pz -= 10.0 * math.cos(h)
    g.frame(1)
g.frame(30)
check(abs(angwrap(view_yaw(g) - v0)) < 0.5, "a knockback does not swing it (view %.1f -> %.1f)" % (v0, view_yaw(g)))
# the game turns her (a hit, the auto-aim) without the player: the camera stays
g.heading += 70.0
g.frame(40)
check(abs(angwrap(view_yaw(g) - v0)) < 0.5, "the game turns her: the camera stays (view %.1f)" % view_yaw(g))
check(boom_side(g, g.tgt()) * side0 > 0 and abs(g.boom() - b0) < 0.5,
      "the side stays (%.0f) and the distance stays (%.0f)" % (boom_side(g, g.tgt()), g.boom()))
# the player turns her (the game's turn action held): with Keep Behind the camera follows her facing
m.ee_set_u32(CHAR + 0xF80, 0x8)
for k in range(20):
    g.heading += 3.0
    g.frame(1)
g.frame(60)
m.ee_set_u32(CHAR + 0xF80, 0)
check(abs(angwrap(view_yaw(g) - g.heading)) < 3.0,
      "the player turns her: Keep Behind brings it behind her (view %.1f, facing %.1f)" % (view_yaw(g), g.heading))
check(g.boom() < 300.5, "never further than its distance (%.0f)" % g.boom())
# a wall pulls the boom in and goes; the game pushes her (no input): it stays in; the game puts her somewhere else (a
# scripted scene): the boom settles at its distance there, never further
g.frame(10)  # the player's last move is past Follow Hold
wall_across_boom(m, g)
g.frame(3)
b_w = g.boom()
m.ee_set_u32(WALL_PTR, 0)
h = math.radians(g.heading)
for k in range(6):
    g.px += 6.0 * math.sin(h)
    g.pz += 6.0 * math.cos(h)
    g.frame(1)
g.frame(40)
check(b_w < 200 and g.boom() <= b_w + 0.5, "wall gone, the game pushes her: no pull-back by itself (%.0f -> %.0f)"
      % (b_w, g.boom()))
g.px += 300.0
g.frame(60)
check(290 < g.boom() < 300.5, "the game puts her elsewhere: the boom settles at its distance (%.0f)" % g.boom())
# Lock On Enemy off: an aim lock of the game does not move the camera; Modern Aim turns her to the camera's view
m.config["lock_on_enemy"] = 0
m.config["rotate_mode"] = 0
g.frame(5)
ex = g.px + 400.0 * math.sin(math.radians(g.heading - 90.0))
ez = g.pz + 400.0 * math.cos(math.radians(g.heading - 90.0))
v1 = view_yaw(g)
logs = len(g.host.logs)
g.host.held = AIM
stage_lock(m, ex, ez)
g.heading = to_target(g, ex, ez)  # the game snaps her to its target
g.frame(40)
check(abs(angwrap(view_yaw(g) - v1)) < 0.5 and not any("aim lock" in s for s in g.host.logs[logs:]),
      "Lock On Enemy off: the camera stays on her (view %.1f -> %.1f), no lock" % (v1, view_yaw(g)))
check(abs(angwrap(g.heading - view_yaw(g))) <= 12.5,
      "Lock On Enemy off: Modern Aim turns her to where the camera looks, as in 2.0.0 (facing %.1f, view %.1f)"
      % (g.heading, view_yaw(g)))
# switched on while the game holds the lock: the camera turns onto the target
m.config["lock_on_enemy"] = 1
g.frame(20)
check(abs(angwrap(view_yaw(g) - to_target(g, ex, ez))) < 2.0,
      "Lock On Enemy on: the camera turns onto the target (view %.1f, target %.1f)"
      % (view_yaw(g), to_target(g, ex, ez)))
m.config["lock_on_enemy"] = 0
v2 = view_yaw(g)
g.frame(20)
check(any(s.endswith("aim lock: released") for s in g.host.logs) and abs(angwrap(view_yaw(g) - g.heading)) < 2.0,
      "switched off during a lock: released, the camera back behind her (view %.1f -> %.1f, facing %.1f)"
      % (v2, view_yaw(g), g.heading))
g.host.held = 0
m.ee[CHAR + 0x08] = 1
m.ee_set_u32(CHAR + 0xF80, 0)
check(not code_touched(m), "no game code read or written")

print("== 29. handed back to a cutscene: the game's own controls - no bind acts, the mouse is free, the gun's own "
      "elevation")
m = new_machine()
g = Game(m)
m.log_writes = m.log_reads = True
g.host.device = 2  # Keyboard & Mouse: while it drives the camera holds the mouse
m.config["cinematic_yield"] = 1
g.frame(5)
check(g.host.mouse_cap == 1 and g.host.requested["camera.freeAim"] == 1, "driving: the mouse held, free aim on")
for _ in range(8):
    m.ee_set_f32(CAM_EYE, 77777.0)
    g.frame(1)
check(g.host.status[0] == 3, "status: handed back to a cutscene")
check(g.host.requested["camera.freeAim"] == 0, "camera.freeAim off: the gun's elevation servo is the game's again")
check(g.host.mouse_cap == 0 and g.host.hidden == 0 and g.host.pad_ovr == 0,
      "the mouse free, no bind hidden from the game, no stick centred")
writes, logs = len(g.host.config_writes), len(g.host.logs)
m.ee_set_u16(CHAR + 0xBC8, 0x2000)  # the game's servo raised the gun (aim up)
g.host.held = SWAP | AIM | CAPTURE  # R3, the left trigger and C pressed while handed back
g.frame(3)
g.host.held = 0
g.frame(3)
check(g.host.status[0] == 3 and g.host.requested["camera.freeAim"] == 0, "still handed back, free aim still off")
check(len(g.host.config_writes) == writes and not any("shoulder" in s or "mouse-look" in s
                                                       for s in g.host.logs[logs:]),
      "Swap Shoulder (R3) and Free Cursor (C) do nothing while handed back: no side saved (%s)"
      % g.host.config_writes[writes:])
check(m.ee_s16(CHAR + 0xBC8) == 0x2000, "the gun's elevation stays the game's while the Aim bind is held (0x%04x)"
      % (m.ee_s16(CHAR + 0xBC8) & 0xFFFF))
check(g.host.mouse_cap == 0 and g.host.hidden == 0 and g.host.pad_ovr == 0,
      "the mouse stays free, nothing hidden or centred after the binds were pressed")
g.frame(20)  # stick neutral
g.host.ly = 1.0
for _ in range(10):
    g.frame(1)
    g.pz += 5.0  # the game moves her
check(g.host.status[0] == 2 and g.host.requested["camera.freeze"] == 1, "she walks: the camera drives again")
check(g.host.requested["camera.freeAim"] == 1 and g.host.mouse_cap == 1, "free aim on again, the mouse held again")
g.host.ly = 0.0
g.frame(3)
g.host.held = SWAP
g.frame(1)
g.host.held = 0
g.frame(1)
check(("shoulder", -50.0) in g.host.config_writes[writes:], "Swap Shoulder works again once it drives (%s)"
      % g.host.config_writes[writes:])
check(not code_touched(m), "no game code read or written")

print("== 30. the left stick walks her where it points from this view: the game's held walking direction (a cut of the "
      "room's camera) is let go while it drives with Turn to Camera Off and Keep Behind Character off")
ACT = CHAR + 0xF78
WALK_HOLD = ACT + 0x32


def walk_held_at_cut(m):
    # The room's camera cut while the stick walked her up: the mapper holds her walking direction (+0x32 = 1, the
    # stick's angle at +0x30); her partner's record holds one too.
    m.ee_set_u32(ACT + 0x30, 0x0001FFFF)
    m.ee_set_u32(ACT + STRIDE + 0x30, 0x0001C000)


def hold_writes(m):
    return [w for w in m.ee_writes if w[0] & ~3 == ACT + 0x30]


m = new_machine()
g = Game(m)
m.log_writes = m.log_reads = True
g.frame(5)
check(not hold_writes(m), "nothing held: nothing written there")
walk_held_at_cut(m)
g.frame(1)
check(m.ee_u16(WALK_HOLD) == 0 and m.ee_u16(ACT + 0x30) == 0xFFFF,
      "defaults, driving: the hold let go at the pad-map mark (+0x32 = %d), the stick's angle word left as it is"
      % m.ee_u16(WALK_HOLD))
check(m.ee_u32(ACT + STRIDE + 0x30) == 0x0001C000, "her partner's record untouched")
n = len(hold_writes(m))
g.frame(5)
check(n == 1 and len(hold_writes(m)) == n, "one write, and nothing more while nothing is held (%d)" % n)
for key, value, what in (("rotate_mode", 1, "Keep Behind Character on"), ("turn_to_camera", 1, "Turn to Camera Moving"),
                         ("turn_to_camera", 2, "Turn to Camera Always"), ("enabled", 0, "the camera's own switch Off")):
    m = new_machine()
    g = Game(m)
    m.config[key] = value
    g.frame(5)
    walk_held_at_cut(m)
    g.frame(3)
    check(m.ee_u16(WALK_HOLD) == 1, "%s: the game's hold stays (as 2.1.0)" % what)
m = new_machine()
g = Game(m)
m.config["cinematic_yield"] = 1
g.frame(5)
for _ in range(8):
    m.ee_set_f32(CAM_EYE, 77777.0)
    g.frame(1)
walk_held_at_cut(m)
g.frame(3)
check(g.host.status[0] == 3 and m.ee_u16(WALK_HOLD) == 1, "handed back to a cutscene: the game's hold stays")
m = new_machine()
g = Game(m)
m.log_writes = m.log_reads = True
g.pad_map = False
g.frame(40)
walk_held_at_cut(m)
g.frame(1)
check(m.ee_u16(WALK_HOLD) == 0, "no pad-map event from the host: let go by the per-frame fallback")
check(not code_touched(m), "no game code read or written")

print("== 26. J's Bar's restroom mirrors: the room reflected at the room's draw while the camera drives there")
ROOM = 0x0049EEF0
OBJ_FLAGS, OBJ_COUNT, OBJ_MATRIX = 0x0049F790, 0x0049F762, 0x0049FA10
MODELS, MATERIALS = 0x00F51000, 0x004EDD90
RECORDS, RECORD_BASE, PACKETS = 0x002941A0, 0x00C00000, 0x00C80000
FN_SET_SLOT, FN_RSTATES, FN_DRAW, FN_SCISSOR = 0x001AF780, 0x00374B20, 0x0018C170, 0x00197490
VIEW = 0x003240E0
CARDS = (63, 66, 69, 72, 75, 78, 81, 84)  # the light bars' glow cards
SEEN, UNSEEN = 0, 1  # objects with a model the glass can / cannot show


def stage_model(m, i, lo, hi):
    """Object i's model record and packet: a DMA "ret" chain with one UNPACK V4-32 block holding a GIF tag of ST,
    RGBAQ, XYZF2 and the 8 corners of the box lo..hi (model space), 4 qwords each, the position first."""
    rec = RECORD_BASE + 0x180 * (100 + i - 1)
    pkt = PACKETS + 0x400 * i
    num = 2 + 4 * 8
    m.ee_set_u32(pkt, 0x60000000 | (num + 2))      # DMA ret, QWC
    m.ee_set_u32(pkt + 24, 0x01000404)             # STCYCL 4, 4
    m.ee_set_u32(pkt + 28, 0x6C008000 | (num << 16))  # UNPACK V4-32
    m.ee_set_u32(pkt + 32, 1)
    m.ee_set_u32(pkt + 48, 0x8000 | 8)             # GIF tag: NLOOP 8, EOP
    m.ee_set_u32(pkt + 52, 0x30000000)             # NREG 3
    m.ee_set_u32(pkt + 56, 0x412)                  # ST, RGBAQ, XYZF2
    for v in range(8):
        q = pkt + 64 + 64 * v
        for k in range(3):
            m.ee_set_f32(q + 4 * k, hi[k] if (v >> k) & 1 else lo[k])
    m.ee_set_u32(pkt + 64 + 64 * 8, 0x17000000)    # MSCNT
    size = 16 * (num + 3)
    m.ee_set_u32(rec + 0x20, pkt)
    m.ee_set_u32(rec + 0x2C, size)
    m.ee_set_u32(rec + 0x40, 8)


def stage_restroom(m):
    """98 objects, every one drawn, one group; the glass (46), its frames (41), the copy (14, 15), the counter top
    (38) and the light bars' glow cards at their places, the others in front of the mirror wall; two objects with a
    model (the rest without one: drawn by every pass); two cuts whose masks hold every object; the frame's size, the
    engine's scissor."""
    m.ee_set_u16(OBJ_COUNT, 98)
    m.ee[ROOM + 3] = 1
    m.ee[ROOM + 4] = 98
    m.ee[ROOM + 0x2B20] = 0
    m.ee_set_u32(ROOM + 0x868, MODELS)
    m.ee_set_u32(RECORDS, RECORD_BASE)
    at = {i: (9700.0 + 2.0 * i, 150.0, 13060.0 + 3.0 * i) for i in range(98)}
    at.update({14: (9490.0, 250.0, 12687.5), 15: (9539.7, 174.8, 12836.2), 38: (9785.0, 131.3, 13066.2),
               41: (9792.6, 210.7, 13025.0), 46: (9792.24, 211.01, 13025.0),
               SEEN: (9900.0, 210.0, 13300.0), UNSEEN: (9200.0, 55.0, 13045.0)})
    for k, i in enumerate(CARDS):
        at[i] = (9689.0 + 22.5 * (k % 4) + (165.0 if k >= 4 else 0.0), 275.0, 13033.5)
    for i in range(98):
        b = OBJ_MATRIX + 0x40 * i
        for k in range(16):
            m.ee_set_f32(b + 4 * k, 1.0 if k in (0, 5, 10, 15) else 0.0)
        if i not in (SEEN, UNSEEN):
            m.ee_set_f32(b + 4, 0.25)  # (an axis with a z part: row 0 = 1, 0.25, 0.5)
            m.ee_set_f32(b + 8, 0.5)
        for k, v in enumerate(at[i]):
            m.ee_set_f32(b + 0x30 + 4 * k, v)
        m.ee[OBJ_FLAGS + i] = 1
        e = MODELS + 0x98 * i
        m.ee_set_u32(e, 100 + i)
        m.ee_set_u32(e + 4, 2)
        m.ee_set_u32(e + 8, 2 * i)
        m.ee_set_u32(e + 12, 2 * i + 1)
        m.ee[e + 0x88] = 1
        m.ee[e + 0x8F] = 1  # (the depth write on)
        m.ee_set_u16(ROOM + 0x68 + 4 * i, 300 + i)
        m.ee_set_u16(ROOM + 0x6A + 4 * i, 400 + i)
    m.ee_set_u16(ROOM + 0x68 + 4 * 3, 300 + 2)  # (object 3's materials are object 2's)
    m.ee_set_u16(ROOM + 0x6A + 4 * 3, 400 + 2)
    stage_model(m, SEEN, (-20.0, -20.0, -20.0), (20.0, 20.0, 20.0))
    stage_model(m, UNSEEN, (-5.0, -5.0, -5.0), (5.0, 5.0, 5.0))
    m.ee[CUT + 1] = 2
    m.ee[CUT + 3] = 0
    for c in range(2):
        for w in range(4):
            m.ee_set_u32(CUT + 0x34 + c * 0x198 + 0x154 + 4 * w, 0xFFFFFFFF if w < 3 else 0x3)
    m.ee_set_u32(0x0029FED8, 512)
    m.ee_set_u32(0x0029FED0, 448)
    for k, v in enumerate((0, 0, 511, 447)):
        m.ee_set_u32(0x0029FDF8 + 4 * k, v)


def build_view(m):
    """The game's view matrix from the camera block, as 0x00375A50 builds it before the room is drawn (rows; columns
    right, up, back; the eye at the origin)."""
    e = [m.ee_f32(CAM_EYE + 4 * i) for i in range(3)]
    t = [m.ee_f32(CAM_TGT + 4 * i) for i in range(3)]
    b = [e[i] - t[i] for i in range(3)]
    n = math.sqrt(sum(v * v for v in b))
    if n < 1e-6:
        return  # (no camera written yet)
    b = [v / n for v in b]
    r = [b[2], 0.0, -b[0]]
    n = math.sqrt(sum(v * v for v in r))
    r = [v / n for v in r]
    u = [b[1] * r[2] - b[2] * r[1], b[2] * r[0] - b[0] * r[2], b[0] * r[1] - b[1] * r[0]]
    for row in range(3):
        for col, axis in enumerate((r, u, b)):
            m.ee_set_f32(VIEW + 16 * row + 4 * col, axis[row])
        m.ee_set_f32(VIEW + 16 * row + 12, 0.0)
    for col, axis in enumerate((r, u, b)):
        m.ee_set_f32(VIEW + 48 + 4 * col, -sum(e[i] * axis[i] for i in range(3)))
    m.ee_set_f32(VIEW + 60, 1.0)


def room_draw(g, room=ROOM, group=0):
    g.host.now, g.host.scratch = [], []
    build_view(g.m)
    g.host.in_room_draw = True
    g.m.call(g.m.symbols["Mcam_OnRoomDraw"], args=(room, group))
    g.host.in_room_draw = False


def mask_bits(m, c, i):
    return (m.ee_u32(CUT + 0x34 + c * 0x198 + 0x154 + 4 * (i >> 5)) >> (i & 31)) & 1


def zwrite(m, i):
    return m.ee[MODELS + 0x98 * i + 0x8F]


m = new_machine()
g = Game(m)
stage_restroom(m)
m.config["cut_match"] = 0
g.px, g.py, g.pz, g.heading = 9790.0, 0.0, 13300.0, 180.0  # in the restroom, facing the mirrors
g.write()
room_draw(g)  # (the game build raises the event: the mod knows the mark exists)
check(not g.host.now and not g.host.scratch, "not driving yet: the room draw draws nothing")
g.frame(5)
check(g.host.status == (2, 0), "driving in the restroom")
check(all(mask_bits(m, c, i) == 0 for c in range(2) for i in (14, 15)) and
      all(mask_bits(m, c, i) == 1 for c in range(2) for i in (0, 46, 75, 97)),
      "the copy (14, 15) is in no cut while the camera drives; the rest stays (the glow card 75 too)")
check(all(m.ee[OBJ_FLAGS + i] & 1 == 0 for i in (14, 15)), "and the current cut does not draw it")
ex = m.ee_f32(CAM_EYE)
left = [i for i in CARDS if ex < m.ee_f32(OBJ_MATRIX + 0x40 * i + 0x30)]
check(left and len(left) < len(CARDS) and all(zwrite(m, i) == (0 if i in left else 1) for i in CARDS),
      "glow cards seen from their left drawn without depth write, the others as the game draws them (eye x %.1f, "
      "left of %s)" % (ex, left))
m.log_writes = m.log_reads = True
room_draw(g)
calls = g.host.now
scissors = [k for k, c in enumerate(calls) if c[0] == FN_SCISSOR]
check(calls and calls[0][0] == FN_SCISSOR and len(calls[0][1]) == 5 and calls[0][1][4] == 0,
      "first the game's scissor routine with five words ($t0 = 0: SCISSOR_1): %s" % (calls[0][1] if calls else None,))
x0, y0, w0, h0 = calls[0][1][:4]
check(0 <= x0 < 512 and 0 <= y0 < 448 and 0 < w0 <= 512 - x0 and 0 < h0 <= 448 - y0 and w0 < 400 and h0 < 300,
      "the scissor is the part of the panes on the screen (%d, %d, %d x %d), inside the frame" % (x0, y0, w0, h0))
check(calls[-1] == (FN_SCISSOR, [0, 0, 512, 448, 0]), "last the engine's scissor back: %s" % (calls[-1][1],))
passes = len(scissors) - 1
draws = [c for c in calls if c[0] == FN_DRAW]
drawn = [c[1][0] - 100 for c in draws]
skipped = {14, 15, 38, 41, 46, UNSEEN} | set(CARDS)
check(passes in (1, 2) and set(drawn) == set(range(98)) - skipped and
      all(drawn.count(i) == passes for i in set(drawn) - {SEEN}) and 1 <= drawn.count(SEEN) <= passes,
      "every object mirrored but the copy, the counter top, what is on the mirror wall and the one whose reflection "
      "the glass cannot show (%d passes, %d draws)" % (passes, len(drawn)))
k = calls.index((FN_DRAW, [100 + SEEN, 0]))
check(calls[k - 4: k] == [(FN_SET_SLOT, [0x1A, SCRATCH]), (FN_RSTATES, [MODELS, ROOM + 0x920]),
                          (FN_SET_SLOT, [0x3A, MATERIALS + 0x4C * 300]), (FN_SET_SLOT, [0x3B, MATERIALS + 0x4C * 400])],
      "an object as the room loop draws it: world matrix, render states, its materials, the model")
k = calls.index((FN_DRAW, [100 + 3, 0]))
check(calls[k - 2: k] == [(FN_SET_SLOT, [0x1A, SCRATCH]), (FN_RSTATES, [MODELS + 0x98 * 3, ROOM + 0x920 + 12])],
      "materials the slots already hold are not set again (object 3 has object 2's)")
mat = [u2f(w) for w in g.host.scratch[drawn.index(2)]]  # (object 2: rows 1, 0.25, 0.5 / 0 1 0 / 0 0 1)
check(abs(mat[2] + 0.5) < 1e-6 and abs(mat[1] - 0.25) < 1e-6 and
      abs(mat[14] - (2 * 13025.0 - 13066.0)) < 1e-3 and abs(mat[12] - 9704.0) < 1e-3 and mat[15] == 1.0,
      "its matrix mirrored about the glass (z = 13025): the axes' z negated, the origin's z to 26050 - z")
check(all(m.ee[OBJ_FLAGS + i] & 1 == 0 for i in (14, 15)), "the copy not drawn with the reflection")
room_draw(g, room=0x00500000)
check(not g.host.now, "another room's draw: nothing")
check(not code_touched(m), "no game code read or written")
m.log_writes = m.log_reads = False
# Show Hidden Objects sets every object's flag at every frame: the copy is still never drawn with the reflection.
m.config["show_hidden_objects"] = 1
g.frame(1)
room_draw(g)
check(len([c for c in g.host.now if c[0] == FN_DRAW]) == len(drawn) and
      all(m.ee[OBJ_FLAGS + i] & 1 == 0 for i in (14, 15)),
      "Show Hidden Objects on: the copy's flags cleared right before the reflection is drawn")
m.config["show_hidden_objects"] = 0
g.frame(1)
# Switched off: the masks, the current cut's flags and the glow cards' bytes as the game's; nothing drawn.
m.config["enabled"] = 0
g.frame(1)
check(all(mask_bits(m, c, i) == 1 for c in range(2) for i in (14, 15, 75)) and
      all(m.ee[OBJ_FLAGS + i] & 1 == 1 for i in (14, 15)),
      "switched off: every mask and the copy's flags back as the game's")
check(all(zwrite(m, i) == 1 for i in CARDS), "switched off: the glow cards' depth write back as the game's")
room_draw(g)
check(not g.host.now and not g.host.scratch, "switched off: the room draw draws nothing")
m.config["enabled"] = 1

print("== 27. cost: MIPS instructions per game frame")
typical = sorted(steps)[len(steps) // 2]
print("  frames %d, median %d, max %d instructions" % (len(steps), typical, max(steps)))
check(max(steps) < 200000, "under 200k instructions per frame (max %d)" % max(steps))

print()
print("FAILED: %d" % len(fails) if fails else "ALL PASSED")
sys.exit(1 if fails else 0)
