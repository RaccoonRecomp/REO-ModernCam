"""Writes patches.json: every code site the camera switches, as a feature-profile fragment for
`REOutbreakTools feature-profile build --mod-patches <this folder>/patches.json` (docs/FEATURES.md of REOutbreak).

    python tools/gen_patches.py

Each site's original instruction is read from YOUR US v2.00 disc files (REO_GAME_DIR, tests/us_image.py) and checked
against the word this port expects; nothing is written when one differs. The file holds addresses and instruction
words of your copy of the game: keep it private (the mod folder, never a repository). The profile builder turns the
originals into xxh64 hashes in the profile it writes.

The sites (Outbreak-ModernCam.exe's File 1 patches, moved to this disc; ro_code_orig*.json has the Japanese words):
- camera.freeze (setting "enabled"): the exe NOPs the cut dispatcher's three handler calls, the follow handler's eye
  call, and turns the follow handler into `jr ra`. v2 keeps the handler-1 call and the follow handler as the exe, and
  narrows handlers 2 and 3 to their camera writes only: the call of the camera init 0x00375650 (which zeroes the eye
  and target every frame) and every store of the handler into the camera block (eye 0x00324138, target 0x00324144,
  FOV 0x00324180, roll 0x0032418C). Their other work (the rail progress 0x003DDDB8-C8,
  the cut record's current eye, the spline lookups) keeps running, so the game's rail state is current when the
  camera hands back. The subtitle probes (camera_v2) show the ad-lib subtitles with either freeze.
- camera.freeAim (settings "modern_aim" and "free_aim"): the gun-elevation servo calls and its four stores, as the exe.
- marks: the game frame (recomp_on_play_main), the pad mapper (recomp_on_pad_map; the exe's VBTN cave) and the
  per-character update where the reload's guest call runs (the exe's reload cave)."""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(HERE, "..", "tests"))
import us_image as us  # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "patches.json")
EXE_SHA256 = "sha256:06bd5fd7243144cd30cdd0c0023763a369738cb91b177b35e85f6a4782b4ca46"
GAME_BIN = "xxh64:1b4bcbcfd81bd910"
JR_RA = 0x03E00008

# (id, image, pc, kind, feature, original word, replacement, option, what)
SITES = [
    # ---- camera.freeze: the dispatcher 0x005BED30 and handler 1 (fixed cut), as the exe [0x0058EAEC]
    ("camera.freeze.h1", "game.bin", 0x005BF06C, "callIf", "camera.freeze", 0x0C16FC7C, None, "enabled",
     "dispatcher: jal 0x005BF1F0 (handler 1, fixed cut: camera init + eye/target/FOV/roll from the cut)"),
    # ---- handler 2 (spline cut, type 8) 0x005BF7B0: the camera writes only [exe: its call 0x0058EAFC]
    ("camera.freeze.h2.init", "game.bin", 0x005BF7E8, "callIf", "camera.freeze", 0x0C0DD594, None, "enabled",
     "handler 2: jal 0x00375650 (camera init: zeroes eye/target, FOV 27, far 50000)"),
] + [
    ("camera.freeze.h2.%s" % n, "game.bin", pc, "subst", "camera.freeze", w, 0, "enabled", "handler 2: " + what)
    for n, pc, w, what in (
        ("eyeX", 0x005BF97C, 0xE4204138, "swc1 f0, eye.x"), ("eyeY", 0x005BF990, 0xE420413C, "swc1 f0, eye.y"),
        ("eyeZ", 0x005BF9A0, 0xE4204140, "swc1 f0, eye.z"),
        ("roll1", 0x005BF9F4, 0xAC22418C, "sw v0, 0x0032418C (delay slot)"),
        ("roll2", 0x005BF9F8, 0xAC24418C, "sw a0, 0x0032418C"),
        ("fov1", 0x005BFA2C, 0xE4204180, "swc1 f0, FOV (delay slot)"), ("fov2", 0x005BFA30, 0xE4204180, "swc1 f0, FOV"),
        ("tgtX", 0x005BFAB8, 0xE4204144, "swc1 f0, target.x"), ("tgtZ", 0x005BFAC4, 0xE420414C, "swc1 f0, target.z"),
        ("tgtY", 0x005BFAD4, 0xE4204148, "swc1 f0, target.y"))
] + [
    # ---- handler 3 (rail cuts) 0x005BF300: the camera writes only [exe: its call 0x0058EB0C]
    ("camera.freeze.h3.init", "game.bin", 0x005BF348, "callIf", "camera.freeze", 0x0C0DD594, None, "enabled",
     "handler 3: jal 0x00375650 (camera init)"),
] + [
    ("camera.freeze.h3.%02d" % i, "game.bin", pc, "subst", "camera.freeze", w, 0, "enabled", "handler 3: " + what)
    for i, (pc, w, what) in enumerate((
        (0x005BF4B8, 0xE4204138, "swc1 f0, eye.x"), (0x005BF4C8, 0xE420413C, "swc1 f0, eye.y"),
        (0x005BF4D8, 0xE4204140, "swc1 f0, eye.z"), (0x005BF52C, 0xAC23418C, "sw v1, roll (delay slot)"),
        (0x005BF530, 0xAC25418C, "sw a1, roll"), (0x005BF564, 0xE4204180, "swc1 f0, FOV (delay slot)"),
        (0x005BF56C, 0xE4204180, "swc1 f0, FOV (delay slot)"), (0x005BF57C, 0xE4204138, "swc1 f0, eye.x"),
        (0x005BF58C, 0xE420413C, "swc1 f0, eye.y"), (0x005BF59C, 0xE4204140, "swc1 f0, eye.z"),
        (0x005BF5A8, 0xAC23418C, "sw v1, roll"), (0x005BF5B4, 0xE4204180, "swc1 f0, FOV"),
        (0x005BF610, 0xE4204144, "swc1 f0, target.x"), (0x005BF620, 0xE420414C, "swc1 f0, target.z (delay slot)"),
        (0x005BF630, 0xE4214144, "swc1 f1, target.x"), (0x005BF638, 0xE420414C, "swc1 f0, target.z"),
        (0x005BF648, 0xE4204148, "swc1 f0, target.y (delay slot)"), (0x005BF660, 0xE4214144, "swc1 f1, target.x"),
        (0x005BF66C, 0xE421414C, "swc1 f1, target.z"), (0x005BF680, 0xE4204148, "swc1 f0, target.y (delay slot)"),
        (0x005BF6B0, 0xE4224144, "swc1 f2, target.x"), (0x005BF6BC, 0xE4204148, "swc1 f0, target.y"),
        (0x005BF6C8, 0xE420414C, "swc1 f0, target.z"), (0x005BF730, 0xE4204138, "swc1 f0, eye.x"),
        (0x005BF73C, 0xE420413C, "swc1 f0, eye.y"), (0x005BF74C, 0xE4204140, "swc1 f0, eye.z (delay slot)"),
        (0x005BF75C, 0xE4204144, "swc1 f0, target.x"), (0x005BF76C, 0xE4204148, "swc1 f0, target.y"),
        (0x005BF77C, 0xE420414C, "swc1 f0, target.z")))
] + [
    # ---- the follow handler 0x005D9960 (door transitions, the character's state 0x0A/0x3A), as the exe
    ("camera.freeze.follow", "game.bin", 0x005D9960, "returnIf", "camera.freeze", 0x27BDFF70, None, "enabled",
     "follow handler entry: addiu sp,sp,-0x90 (the exe writes jr ra; nop). Reached only through a callback pointer "
     "(registered by jal 0x00573890 at 0x005BEA84/0x005BEB40), so no call site can be switched: returnIf"),
    ("camera.freeze.followEye", "game.bin", 0x005D9AC0, "callIf", "camera.freeze", 0x0C0DC140, None, "enabled",
     "follow handler: jal 0x00370500 (eye = offset + target), as the exe [0x005A91E0]"),
    # ---- camera.freeAim: the gun-elevation servo and its stores to the character's +0xBC8, as the exe
    ("camera.freeAim.servo1", "game.bin", 0x00586C08, "callIf", "camera.freeAim", 0x0C0DDB74, None,
     "modern_aim, free_aim", "jal 0x00376DD0 (gun-elevation servo) [0x00556968]"),
    ("camera.freeAim.servo2", "game.bin", 0x00586C20, "callIf", "camera.freeAim", 0x0C0DDB74, None,
     "modern_aim, free_aim", "jal 0x00376DD0 [0x00556980]"),
    ("camera.freeAim.servo3", "game.bin", 0x00586C38, "callIf", "camera.freeAim", 0x0C0DDB74, None,
     "modern_aim, free_aim", "jal 0x00376DD0 [0x00556998]"),
    ("camera.freeAim.store1", "game.bin", 0x00586C6C, "subst", "camera.freeAim", 0xA6200BC8, 0,
     "modern_aim, free_aim", "sh zero, 0xBC8(s1) [0x005569CC]"),
    ("camera.freeAim.store2", "game.bin", 0x00586C78, "subst", "camera.freeAim", 0xA6230BC8, 0,
     "modern_aim, free_aim", "sh v1, 0xBC8(s1) [0x005569D8]"),
    ("camera.freeAim.store3", "game.bin", 0x00586C84, "subst", "camera.freeAim", 0xA6230BC8, 0,
     "modern_aim, free_aim", "sh v1, 0xBC8(s1) [0x005569E4]"),
    ("camera.freeAim.store4", "game.bin", 0x00586C88, "subst", "camera.freeAim", 0xA6200BC8, 0,
     "modern_aim, free_aim", "sh zero, 0xBC8(s1) [0x005569E8]"),
]

# (id, image, pc, original word, hook, what)
MARKS = [
    ("camera.gameFrame", "exe", 0x0019F6C0, 0x27BDFFF0, "game.frame",
     "the game's frame mark (shared with the rate profile's frame mark): recomp_on_play_main, once per game frame"),
    ("camera.padMap", "exe", 0x001B0C5C, 0x8CCC0000, "camera.padMap",
     "the pad mapper's lw t4,0(a2) [the exe's VBTN hook 0x001AFD38]: recomp_on_pad_map(a2, s0) before the load"),
    ("camera.reload", "game.bin", 0x0066B500, 0x27BDFFE0, "camera.reload",
     "entry of the per-character update (a0 = the character record) [the exe's reload hook 0x006399A0]: the queued "
     "guest call of the item-combine routine 0x006690E0 runs here (reo_guest_call_at)"),
]


def load_ram():
    ram = bytearray(32 * 1024 * 1024)
    for va, data, _ in us.elf(os.path.join(us.GAME, "SLUS_207.65")):
        if data:
            ram[va:va + len(data)] = data
    for n in (0, 2):
        o = us.overlay(n)
        ram[o["load"]:o["load"] + len(o["image"])] = o["image"]
    return ram


def main():
    ram = load_ram()
    word = lambda a: struct.unpack_from("<I", ram, a)[0]  # noqa: E731
    problems = []
    sites = []
    for sid, image, pc, kind, feature, orig, repl, option, what in SITES:
        found = word(pc)
        if found != orig:
            problems.append("%s 0x%08X: the disc has 0x%08X, expected 0x%08X" % (sid, pc, found, orig))
        site = {"id": sid, "image": image, "pc": "0x%08X" % pc, "kind": kind, "when": feature,
                "original": "0x%08X" % orig}
        if kind == "subst":
            site["replacement"] = "0x%08X" % repl
        site["option"] = option
        site["note"] = what
        sites.append(site)
    for sid, image, pc, orig, hook, what in MARKS:
        found = word(pc)
        if found != orig:
            problems.append("%s 0x%08X: the disc has 0x%08X, expected 0x%08X" % (sid, pc, found, orig))
        sites.append({"id": sid, "image": image, "pc": "0x%08X" % pc, "kind": "mark", "hook": hook,
                      "original": "0x%08X" % orig, "option": "enabled", "note": what})
    if problems:
        print("\n".join(problems))
        raise SystemExit("patches.json not written: the disc does not hold the expected words")
    ids = [s["id"] for s in sites]
    assert len(ids) == len(set(ids))
    fragment = {
        "schemaVersion": 1,
        "profile": "features",
        "note": "PRIVATE: REO_ModernCam's code sites on Resident Evil Outbreak (NTSC-U) SLUS-20765 v2.00, from your own "
                "disc (tools/gen_patches.py). Merge with REOutbreakTools feature-profile build --mod-patches; never "
                "commit. 'option' names the mod setting that switches the feature, 'note' what the site is.",
        "sources": ["Outbreak-ModernCam.exe 1.2.0 (Snippy) File 1 patches, ported to the US disc"],
        "images": {"exe": EXE_SHA256, "game.bin": GAME_BIN},
        "features": [
            {"name": "camera.freeze", "title": "Modern Camera: the game's camera held", "group": "Modern Camera",
             "description": "The game's camera handlers stop writing the camera (eye, target, FOV, roll); the Modern "
                            "Camera writes it every frame. Handler 1 and the follow handler are skipped as in "
                            "ModernCam; handlers 2 and 3 keep everything but their camera writes."},
            {"name": "camera.freeAim", "title": "Modern Camera: free gun elevation", "group": "Modern Camera",
             "description": "The gun-elevation servo and its stores are skipped, so the gun follows the camera's "
                            "pitch instead of snapping to the game's three angles."},
        ],
        "sites": sites,
    }
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(fragment, indent=2) + "\n")
    n = {k: sum(1 for s in sites if s.get("when") == k) for k in ("camera.freeze", "camera.freeAim")}
    print("patches.json: %d camera.freeze sites, %d camera.freeAim sites, %d marks; every original matches the disc"
          % (n["camera.freeze"], n["camera.freeAim"], len(MARKS)))


if __name__ == "__main__":
    main()
