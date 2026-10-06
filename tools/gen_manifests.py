"""Writes mod.toml, mod.json, manifest.json and window_layout.json from tools/settings.py:

    python tools/gen_manifests.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
from settings import S, EXE  # noqa: E402

OUT = os.path.dirname(HERE)
VERSION = "2.0.0"

NAME = "Resident Evil Outbreak — Modern Camera"
SHORT = "Over-the-shoulder camera for Resident Evil Outbreak (NTSC-U v2.00)"
DESCRIPTION = """A port of Snippy's Outbreak ModernCam to the recompilation of the US release (SLUS-20765, disc version 2.00).

Replaces the fixed camera angles with an over-the-shoulder camera you turn with the right stick or the mouse:
- Main, Aim and Alt framings, blended as you raise the weapon
- Free gun elevation that follows the camera, walking while aiming, a crosshair
- Reload, sprint, shoot and special-action buttons, shoulder swap, recenter
- Camera collision against the room's walls
- Borrows the nearest room camera so the game still draws what the view needs

Every setting can be changed while playing. Outbreak ModernCam is Snippy's work (heysnippy.com); ask Snippy before
publishing this port."""

HOST_DESCRIPTION = (
    "Over-the-shoulder camera for the US release (SLUS-20765 v2.00), ported from Snippy's Outbreak ModernCam "
    "(made for the Japanese discs; heysnippy.com). Right-stick and mouse look, Main/Aim/Alt framings, free gun "
    "elevation, walking while aiming, crosshair, reload, sprint and shoot buttons, camera collision and room-cut "
    "matching.\n\n"
    "It needs the game's mod runtime, the host services of HOST_API.md and the game code built with this mod's "
    "patches.json (Build-Game with the feature profile). Ask Snippy before publishing it."
)

TABS = ["Header", "Camera", "Look", "Aiming", "Controls", "Advanced"]


def toml_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def fmt_num(v):
    if isinstance(v, float) and v.is_integer():
        v = int(v)
    return repr(v)


def check():
    keys = [st["key"] for st in S]
    assert len(keys) == len(set(keys)), "duplicate keys"
    missing = [k for k in EXE if k not in keys]
    assert not missing, "exe keys without a setting: %s" % missing
    assert len(EXE) == 89 and len(set(EXE)) == 89
    for st in S:
        if st["kind"] in ("num", "int"):
            assert st["min"] <= st["default"] <= st["max"], st["key"]
        if st["kind"] == "choice":
            assert st["default"] in st["options"], st["key"]
        if st["kind"] == "string":
            assert len(st["default"]) <= st["maxlen"], st["key"]
        assert st["tab"] in TABS, st["key"]


def write_toml():
    lines = []
    lines.append("[manifest]")
    lines.append('id = "REO_ModernCam"')
    lines.append(f'version = "{VERSION}"')
    lines.append("display_name = " + toml_str(NAME))
    lines.append('description = """\n' + DESCRIPTION + '\n"""')
    lines.append("short_description = " + toml_str(SHORT))
    lines.append('authors = [ "RaccoonRecomp", "Snippy (Outbreak ModernCam)" ]')
    lines.append("# Must match the game_id the recomp registers for the regular (black-label) build, SLUS-20765 v2.00.")
    lines.append('game_id = "reo"')
    lines.append('minimum_recomp_version = "1.0.0"')
    lines.append("dependencies = []")
    lines.append("native_libraries = []")
    lines.append("")
    lines.append("[inputs]")
    lines.append('elf_path = "build/mod.elf"')
    lines.append('mod_filename = "REO_ModernCam"')
    lines.append("# Function reference symbols for SLUS-20765 v2.00 (this mod references no game functions).")
    lines.append('func_reference_syms_file = "syms/reo.syms.toml"')
    lines.append("data_reference_syms_files = []")
    lines.append('additional_files = [ "thumb.png" ]')
    for st in S:
        lines.append("")
        lines.append("[[manifest.config_options]]")
        lines.append("id = " + toml_str(st["key"]))
        lines.append("name = " + toml_str(st["label"]))
        lines.append("description = " + toml_str(st["desc"]))
        if st["kind"] == "bool":
            lines.append('type = "Enum"')
            lines.append('options = [ "Off", "On" ]')
            lines.append("default = " + toml_str("On" if st["default"] else "Off"))
        elif st["kind"] == "choice":
            lines.append('type = "Enum"')
            lines.append("options = [ " + ", ".join(toml_str(o) for o in st["options"]) + " ]")
            lines.append("default = " + toml_str(st["default"]))
        elif st["kind"] == "string":
            lines.append('type = "String"')
            lines.append("default = " + toml_str(st["default"]))
        else:
            prec = st.get("prec", 0)
            step = st.get("step", 1)
            lines.append('type = "Number"')
            lines.append("min = " + fmt_num(st["min"]))
            lines.append("max = " + fmt_num(st["max"]))
            lines.append("step = " + fmt_num(step))
            lines.append("precision = " + str(prec))
            lines.append("percent = false")
            lines.append("default = " + fmt_num(st["default"]))
    with open(os.path.join(OUT, "mod.toml"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def write_mod_json():
    settings = []
    for st in S:
        d = {"key": st["key"]}
        if st["kind"] == "bool":
            d["type"] = "boolean"
            d["default"] = bool(st["default"])
        elif st["kind"] == "choice":
            d["type"] = "choice"
            d["default"] = st["default"]
            d["choices"] = st["options"]
        elif st["kind"] == "int":
            d["type"] = "integer"
            d["default"] = int(st["default"])
            d["min"] = int(st["min"])
            d["max"] = int(st["max"])
        elif st["kind"] == "string":
            d["type"] = "string"
            d["default"] = st["default"]
            d["maxLength"] = st["maxlen"]
        else:
            d["type"] = "number"
            d["default"] = st["default"]
            d["min"] = st["min"]
            d["max"] = st["max"]
        d["label"] = st["label"]
        if st["desc"]:
            d["description"] = st["desc"]
        settings.append(d)
    manifest = {
        "id": "reo-modern-camera",
        "name": NAME,
        "version": VERSION,
        "author": "RaccoonRecomp",
        "authors": ["Snippy (Outbreak ModernCam)"],
        "description": HOST_DESCRIPTION,
        "icon": "icon.png",
        "game": {"id": "resident-evil-outbreak-file-1", "revisions": ["slus-20765-v2.00"]},
        "apiVersion": 1,
        "settings": settings,
    }
    with open(os.path.join(OUT, "mod.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")


def write_manifest_ts():
    manifest_ts = {
        "name": "REO_ModernCam",
        "version_number": VERSION,
        "website_url": "",
        "description": SHORT,
        "dependencies": [],
    }
    with open(os.path.join(OUT, "manifest.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(manifest_ts, indent=4) + "\n")


def write_layout():
    """For the camera window (HOST_API.md, "The settings window"): the exe's tabs and groups, in order."""
    tabs = []
    for tab in TABS:
        groups = []
        for st in S:
            if st["tab"] != tab:
                continue
            name = st["group"] or tab
            g = next((x for x in groups if x["group"] == name), None)
            if g is None:
                g = {"group": name, "settings": []}
                groups.append(g)
            row = {"key": st["key"]}
            if st["widget"]:
                row["widget"] = st["widget"]
            if st["key"] in EXE:
                row["exe"] = True
            g["settings"].append(row)
        tabs.append({"tab": tab, "advanced": tab == "Advanced", "groups": groups})
    layout = {
        "schemaVersion": 1,
        "mod": "reo-modern-camera",
        "version": VERSION,
        "note": "Generated by tools/gen_manifests.py from tools/settings.py. The exe's window shows the Camera, Look, "
                "Aiming and Controls tabs; Advanced holds the values the exe reads but hides. 'exe' marks the 89 "
                "values of Outbreak-ModernCam.exe.",
        "credit": "Modern Camera: Outbreak ModernCam by Snippy (heysnippy.com), ported for the USA disc",
        "tabs": tabs,
    }
    with open(os.path.join(OUT, "window_layout.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(layout, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    check()
    write_toml()
    write_mod_json()
    write_manifest_ts()
    write_layout()
    print(len(S), "settings written;", len([k for k in EXE if k in [s["key"] for s in S]]), "of the exe's 89")
