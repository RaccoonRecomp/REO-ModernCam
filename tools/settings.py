# Single source for REO_ModernCam's settings (v2): generates mod.toml config_options, mod.json settings and
# window_layout.json (tools/gen_manifests.py).
#
# Every value Outbreak-ModernCam.exe reads for File 1 (its 89 defaults, ro_camera_config.py, with the File 1 seed of
# app/default_settings.json on top) is here under the exe's own key, default and range, plus the port's extras
# (enabled, the Aim/Alt rig values with their "custom" switches, draw_distance, show_hidden_objects).
# EXE lists the 89 exe keys; tests/test_driver.py checks that each one is defined here.
#
# Each entry: key, label, kind (bool | num | int | choice | string), default, range, description, and where the camera
# window shows it: tab (Camera, Look, Aiming, Controls, Advanced), group, and a widget hint:
#   bind     - a ModernCam bind code ("trig:L", "trig:R", "pad:0x4000", "key:0x43", "mb:0x02", "" = none)
#   device   - input_device: "auto", "kbm" or "pad:N"
#   color    - "#rrggbb"
#   inherit:K - an Aim/Alt rig value that follows Main's value K until its "_set" switch is on

ONOFF = ["Off", "On"]


def B(key, label, default, desc="", tab="Advanced", group="", widget=""):
    return dict(key=key, label=label, kind="bool", default=default, desc=desc, tab=tab, group=group, widget=widget)


def N(key, label, default, mn, mx, step, prec, desc="", tab="Advanced", group="", widget=""):
    return dict(key=key, label=label, kind="num", default=default, min=mn, max=mx, step=step, prec=prec, desc=desc,
                tab=tab, group=group, widget=widget)


def I(key, label, default, mn, mx, desc="", tab="Advanced", group="", widget=""):
    return dict(key=key, label=label, kind="int", default=default, min=mn, max=mx, desc=desc, tab=tab, group=group,
                widget=widget)


def C(key, label, options, default, desc="", tab="Advanced", group="", widget=""):
    return dict(key=key, label=label, kind="choice", options=options, default=default, desc=desc, tab=tab,
                group=group, widget=widget)


def T(key, label, default, maxlen, desc="", tab="Advanced", group="", widget=""):
    return dict(key=key, label=label, kind="string", default=default, maxlen=maxlen, desc=desc, tab=tab, group=group,
                widget=widget)


def rig(prefix, name, tab_desc):
    """The Aim or Alt rig: each of ModernCam's six rig values with the switch that makes it apply (the exe applies an
    aim_/alt_ key as soon as it is set; unset, the value follows Main)."""
    out = []
    for k, label, d, mn, mx, step, prec, main_desc in (
            ("distance", "Distance", 300, 100, 600, 5, 0, "How far behind her the camera sits."),
            ("shoulder", "Left / Right", 50, -150, 150, 5, 0, "Sideways offset, + = her right shoulder."),
            ("height", "Height", 130, 50, 250, 5, 0, "Lifts the camera and its look point together."),
            ("fov_deg", "Field of View", 60, 0, 90, 1, 0, "Vertical field of view. 0 keeps the room's own FOV."),
            ("look_drop", "Look Drop", 0, -150, 150, 5, 0, "How far below its own height the camera looks."),
            ("pitch_trim", "Pitch Trim", 0, -45, 45, 1, 0, "Constant tilt added to the orbit pitch.")):
        adv = k in ("look_drop", "pitch_trim")
        tab = "Advanced" if adv else "Camera"
        group = ("Framing tilt: " if adv else "Rig: ") + name
        out.append(B(f"{prefix}_{k}_set", f"{name}: Custom {label}", False,
                     f"On: {tab_desc} uses its own {label.lower()} below. Off: it follows the main camera's.",
                     tab=tab, group=group, widget=f"inherit-switch:{prefix}_{k}"))
        out.append(N(f"{prefix}_{k}", f"{name}: {label}", d, mn, mx, step, prec,
                     f"{main_desc} Applies while {name}: Custom {label} is on.", tab=tab, group=group,
                     widget=f"inherit:{k}"))
    return out


S = [
    B("enabled", "Modern Camera", True,
      "Over-the-shoulder camera (the exe's Start/Stop). Off hands the camera and everything else back to the game.",
      tab="Header"),
    # ---- Camera tab (the exe's window) --------------------------------------------------------------------------
    N("distance", "Distance", 300, 100, 600, 5, 0, "How far behind her the camera sits.", tab="Camera",
      group="Rig: Main"),
    N("shoulder", "Left / Right", 50, -150, 150, 5, 0,
      "Sideways offset. Positive = over her right shoulder, negative = left. Swap Shoulder writes the new side here.",
      tab="Camera", group="Rig: Main"),
    N("height", "Height", 130, 50, 250, 5, 0,
      "Raises the camera and the point it looks at together, so the view moves up or down without tilting.",
      tab="Camera", group="Rig: Main"),
    N("fov_deg", "Field of View", 60, 0, 90, 1, 0, "Vertical field of view in degrees (the exe: 30-90). 0 keeps each "
      "room's own FOV.", tab="Camera", group="Rig: Main"),
    N("look_drop", "Look Drop", 0, -150, 150, 5, 0,
      "How far below its own height the camera looks: a fixed downward framing tilt (the exe keeps it out of its "
      "window).", group="Framing tilt: Main"),
    N("pitch_trim", "Pitch Trim", 0, -45, 45, 1, 0, "Constant up/down tilt added to the camera's orbit pitch.",
      group="Framing tilt: Main"),
] + rig("aim", "Aim", "the aim camera (blended in while aiming)") + rig("alt", "Alt", "the Alt View camera") + [
    B("rotate_mode", "Keep Behind Character", False,
      "On (the exe's 'follow'): the camera swings back behind her as she moves. Off ('manual', ModernCam's File 1 "
      "default): it stays wherever you leave it.", tab="Camera", group="Following"),
    N("follow_strength", "Follow Speed", 0.12, 0.02, 0.4, 0.01, 2,
      "How quickly the camera swings behind her (Keep Behind Character on).", tab="Camera", group="Following"),
    N("free_look_return", "Recenter Speed", 1.2, 0, 4, 0.1, 1,
      "Seconds for a look with the right stick or mouse to ease back behind her (Keep Behind Character on). 0 never "
      "releases it.", tab="Camera", group="Following"),
    # ---- Look tab -----------------------------------------------------------------------------------------------
    N("stick_speed", "Sensitivity (gamepad)", 140, 40, 300, 5, 0,
      "Turn speed in degrees per second at full right-stick push.", tab="Look", group="Look"),
    N("mouse_sensitivity", "Sensitivity (keyboard & mouse)", 0.15, 0.02, 0.6, 0.01, 2,
      "Degrees per mouse count x 0.4, as the exe. Raw mouse motion, no acceleration.", tab="Look", group="Look"),
    B("invert_pitch", "Invert Up/Down", False, "", tab="Look", group="Look"),
    # ---- Aiming tab ---------------------------------------------------------------------------------------------
    B("aim_pitch_lock", "Lock Look to the Gun", True,
      "While aiming, stop the camera tilting further than the gun can follow.", tab="Aiming", group="Aiming"),
    N("aim_sensitivity", "Aim Sensitivity", 0.55, 0.1, 1.5, 0.05, 2, "Look speed multiplier while aiming.",
      tab="Aiming", group="Aiming"),
    N("gun_pitch_max", "Gun Elevation", 45, 5, 45, 1, 0, "How far up and down the gun can point, in degrees.",
      tab="Aiming", group="Aiming"),
    B("crosshair", "Crosshair", True, "Show a crosshair while aiming (drawn by REO over the game picture).",
      tab="Aiming", group="Reticle"),
    C("crosshair_style", "Crosshair Shape", ["Dot", "Cross"], "Dot", "", tab="Aiming", group="Reticle"),
    I("crosshair_size", "Crosshair Size", 3, 1, 16, "Pixels: the dot's radius or the cross's arm length.",
      tab="Aiming", group="Reticle"),
    I("crosshair_gap", "Crosshair Gap", 3, 0, 16, "Pixels between the centre and each arm (Cross only).",
      tab="Aiming", group="Reticle"),
    I("crosshair_thickness", "Crosshair Thickness", 2, 1, 6, "Pixels (Cross only).", tab="Aiming", group="Reticle"),
    # ---- Controls tab -------------------------------------------------------------------------------------------
    T("input_device", "Steered By", "auto", 16,
      "auto (Gamepad: the most active pad), kbm (Keyboard & Mouse) or pad:N (that pad). The camera's own input; "
      "the game keeps REO's controls.", tab="Controls", group="Input", widget="device"),
    T("aim_button", "Aim", "trig:L", 32,
      "Hold to aim (ModernCam's File 1 bind: left trigger). With Modern Aim the camera holds R1, the game's aim, "
      "for her. On Keyboard & Mouse a pad or trigger bind is the right mouse button.",
      tab="Controls", group="Binds", widget="bind"),
    T("shoot_button", "Shoot", "trig:R", 32,
      "Fires with whichever button fires under your control type (File 1 bind: right trigger).",
      tab="Controls", group="Binds", widget="bind"),
    T("reload_button", "Reload", "pad:0x4000", 32,
      "Reloads the equipped weapon from carried ammo with the game's own reload animation (File 1 bind: X / "
      "Square).", tab="Controls", group="Binds", widget="bind"),
    T("sprint_button", "Sprint (toggle)", "pad:0x0040", 32,
      "Sprint without holding the dash button; takes the dash action off its original button (File 1 bind: L3).",
      tab="Controls", group="Binds", widget="bind"),
    T("capture_button", "Free Cursor", "key:0x43", 32,
      "Keyboard & Mouse: frees or captures the mouse (C).", tab="Controls", group="Binds", widget="bind"),
    T("recenter_button", "Recenter", "", 32, "Snaps the camera behind her.", tab="Controls", group="Binds",
      widget="bind"),
    T("swap_shoulder_button", "Swap Shoulder", "pad:0x0080", 32,
      "Moves the camera to her other shoulder and saves the side (File 1 bind: R3).", tab="Controls",
      group="Binds", widget="bind"),
    T("special_button", "Special Action", "", 32,
      "While held, presses the dash action's button for the game (the exe's note: for characters whose special "
      "move shares the dash button), and keeps the bound button from the game.", tab="Controls", group="Binds",
      widget="bind"),
    T("lens_button", "Watch Next Player", "", 32,
      "Multiplayer only: the camera follows the next human player while you keep control. Offline there is no one "
      "else to watch, so it does nothing (online play is not available).", tab="Controls", group="Binds",
      widget="bind"),
    T("switch_view_button", "Alt View", "", 32, "Toggles the Alt rig.", tab="Controls", group="Binds",
      widget="bind"),
    # ---- Advanced: every value the exe reads but its window hides -----------------------------------------------
    N("pitch_speed", "Vertical Sensitivity", 90, 20, 300, 5, 0,
      "Up/down speed in degrees per second at full right-stick push.", group="Look"),
    B("invert_stick", "Invert Left/Right", False, "", group="Look"),
    N("pitch_min", "Lowest Look Angle", -35, -89, 0, 1, 0, "", group="Look"),
    N("pitch_max", "Highest Look Angle", 70, 0, 89, 1, 0, "", group="Look"),
    N("aim_blend", "Aim Zoom Speed (fallback)", 0.25, 0.02, 1, 0.01, 2,
      "Used for zooming in or out when that speed below is 0 (the exe: when it is not set).", group="Aim tuning"),
    N("aim_blend_in", "Aim Zoom In Speed", 0.35, 0, 1, 0.01, 2, "0 uses the fallback above.", group="Aim tuning"),
    N("aim_blend_out", "Aim Zoom Out Speed", 0.15, 0, 1, 0.01, 2, "0 uses the fallback above.", group="Aim tuning"),
    B("aim_follow", "Follow While Aiming", True, "Used when Modern Aim is off.", group="Aim tuning"),
    N("aim_follow_strength", "Follow Speed While Aiming", 0.3, 0.02, 1, 0.01, 2, "", group="Aim tuning"),
    B("aim_free_look", "Free Look While Aiming", True, "", group="Aim tuning"),
    B("aim_snap_behind", "Snap Behind on Aim", True,
      "Pressing aim swings the camera straight behind her (Modern Aim off).", group="Aim tuning"),
    N("aim_snap_speed", "Snap Speed", 0.5, 0.1, 1, 0.05, 2, "", group="Aim tuning"),
    N("aim_zero_deg", "Gun Zero", -0.1, -45, 45, 0.1, 1, "Gun elevation when the camera is level.",
      group="Aim tuning"),
    N("aim_shoulder_min", "Aim Shoulder Minimum", 0, 0, 150, 5, 0, "Minimum sideways offset while aiming.",
      group="Aim tuning"),
    N("modern_aim_deadzone", "Aim Turn Deadzone", 12, 0, 45, 1, 0,
      "While aiming, how many degrees the camera and her facing may differ before she turns.", group="Aim tuning"),
    B("modern_aim", "Modern Aim", True,
      "Aim where the camera looks: she turns with the camera while aiming and the gun follows it.",
      group="Aim tuning"),
    B("free_aim", "Free Gun Elevation", True,
      "The gun follows the camera up and down instead of snapping to the game's three angles.", group="Aim tuning"),
    N("aim_strafe_speed", "Strafe Speed While Aiming", 45, 0, 120, 5, 0,
      "File 1 roots her when she aims; the camera moves her with the left stick at this speed. 0 turns it off.",
      group="Aim tuning"),
    C("turn_to_camera", "Turn to Camera", ["Off", "Moving", "Always"], "Off",
      "Moving: she faces the direction you push relative to the camera. Always: she always faces where the camera "
      "looks.", group="Turning"),
    N("turn_speed", "Turn Speed", 165, 30, 720, 5, 0, "Degrees per second when the camera turns her.",
      group="Turning"),
    N("turn_deadzone", "Turn Deadzone", 10, 1, 45, 1, 0, "", group="Turning"),
    B("own_turn", "Own Turning", False,
      "File 2 only in the exe (its heading freeze); File 1 has none, so this changes nothing - as in the exe.",
      group="Turning"),
    N("follow_hold", "Follow Hold", 0.18, 0, 1, 0.01, 2,
      "Seconds she counts as still moving after her last step (the follow heading).", group="Following"),
    C("follow_dir", "Camera Side", ["Behind her", "In front of her"], "Behind her",
      "Where Keep Behind, snap and recenter put the camera (the exe's follow_dir 1 / -1).", group="Following"),
    N("yaw_offset", "Yaw Offset", 0, -180, 180, 1, 0, "Degrees added to the follow and snap direction.",
      group="Following"),
    N("shoulder_swap_time", "Shoulder Swap Speed", 0.18, 0.02, 1, 0.01, 2,
      "How quickly the camera slides to the other shoulder.", group="Following"),
    N("recenter_time", "Recenter Time", 0.25, 0, 2, 0.05, 2, "Seconds the Recenter button eases the camera back.",
      group="Following"),
    B("cam_collision", "Camera Collision", True, "Pulls the camera in front of walls.", group="Collision"),
    N("cam_margin", "Wall Margin", 55, 0, 150, 5, 0, "How far in front of a wall the camera stops.",
      group="Collision"),
    N("cam_block_top", "Low Obstacle Height", 25, 0, 150, 5, 0,
      "Walls whose top is below the look point plus this do not pull the camera in.", group="Collision"),
    B("cam_ignore_props", "Ignore Props", True,
      "Skip walls flagged as props. File 1's wall data has no prop flag, so this changes nothing - as in the exe.",
      group="Collision"),
    N("cam_recover", "Collision Recover Speed", 0.1, 0.02, 1, 0.01, 2, "", group="Collision"),
    N("cam_hold", "Collision Hold", 0.12, 0, 1, 0.01, 2, "Seconds the camera stays pulled in after a wall clears.",
      group="Collision"),
    N("cam_snap", "Collision Ease In", 0, 0, 1, 0.05, 2, "0 pulls in instantly; higher values ease in.",
      group="Collision"),
    N("near_clip", "Near Value", 50, 0, 500, 5, 0,
      "The exe's near_clip, written to the camera block (0 keeps the game's). On this disc that word is the fixed "
      "camera's look distance, so it has no visible effect while the camera drives.", group="Projection"),
    N("far_clip", "Far Clip", 0, 0, 200000, 1000, 0,
      "The projection's far plane. 0 keeps the game's (50,000). Larger values show nothing more: the game's "
      "scenes, sky included, lie within 50,000. Smaller values cut off distant scenery.", group="Projection"),
    N("fov_scale", "Horizontal Scale", 0, 0, 3, 0.01, 2,
      "The projection's horizontal scale (the game uses 1.33). 0 keeps the game's, and REO's widescreen setting.",
      group="Projection"),
    B("cut_match", "Match Room Cuts", True,
      "Borrows the room's fixed camera nearest the view, so the game draws what that angle needs.", group="World"),
    N("cut_match_radius", "Cut Match Radius", 250, 50, 1000, 10, 0, "", group="World"),
    I("cut_match_max_cuts", "Cut Match Max Cuts", 6, 1, 64, "Rooms with more cuts than this are not matched.",
      group="World"),
    B("show_hidden_objects", "Show Hidden Objects", False, "Keeps objects the fixed camera would hide visible.",
      group="World"),
    N("draw_distance", "Draw Distance", 1.0, 0.1, 1, 0.1, 1,
      "Below 1: shrinks the radii within which the game hides models close to the camera, as the exe. It stops at "
      "1: on this disc the exe's values above 1 only enlarge those radii, which hides nearby characters, and there "
      "is no farther view to open (the game's scenes, sky included, lie within its far plane).", group="World"),
    B("char_light", "Character Light Override", False, "", group="World"),
    T("char_light_color", "Character Light Colour", "#808080", 9, "", group="World", widget="color"),
    B("door_hold", "Hold Camera Through Doors", True,
      "When the game moves the camera (doors, scripted shots), take it back.", group="Doors and cutscenes"),
    B("cinematic_yield", "Yield During Cutscenes", False,
      "Hand the camera back while the game runs its own cinematic instead of fighting it; returns when you move "
      "again.", group="Doors and cutscenes"),
    I("cinematic_yield_takes", "Cutscene Detection Takes", 3, 2, 10, "", group="Doors and cutscenes"),
    N("cinematic_yield_window", "Cutscene Detection Window", 1.0, 0.2, 5, 0.1, 1, "Seconds.",
      group="Doors and cutscenes"),
    I("cinematic_neutral_ms", "Resume Delay (ms)", 400, 50, 2000,
      "How long the stick must rest before moving resumes the camera.", group="Doors and cutscenes"),
    I("cinematic_move_confirm_ms", "Move Confirm (ms)", 180, 0, 2000,
      "A dormant value of the exe: it is defined but never read, so it changes nothing.",
      group="Doors and cutscenes"),
    C("sprint_mode", "Sprint Mode", ["Toggle", "Hold"], "Toggle", "Toggle stops when you stop moving.",
      group="Input"),
    B("own_look_stick", "Right Stick Only Moves the Camera", True,
      "The right stick is the game's ad-lib control. On, the game sees it centred, so looking around does not "
      "trigger ad-libs (not on Keyboard & Mouse, as the exe).", group="Input"),
    N("seat_hold_seconds", "Seat Detection Wait", 4, 0, 30, 0.5, 1,
      "The exe's online seat detection (which character is yours). REO knows the local player, so this changes "
      "nothing.", group="Input"),
    B("reload_animated", "Animated Reload", True,
      "The reload uses the game's own combine routine and animation. Off: no reload (this exe version has no "
      "other reload).", group="Reload"),
    T("crosshair_color", "Crosshair Colour", "#f2f2f2", 9, "", group="Reticle", widget="color"),
    I("crosshair_dx", "Crosshair Offset X", 0, -200, 200, "Pixels, + = right.", group="Reticle"),
    I("crosshair_dy", "Crosshair Offset Y", 0, -200, 200, "Pixels, - = up.", group="Reticle"),
    T("reveal_button", "Reveal Camera", "", 32,
      "A capture aid hidden in the exe: blends between the room's own fixed camera and this one.",
      group="Reveal camera", widget="bind"),
    N("reveal_time", "Reveal Speed", 1.6, 0.2, 6, 0.1, 1, "Seconds for the reveal blend.", group="Reveal camera"),
    B("debug_log", "Camera Event Log", False, "Write the camera's events ([evt] lines) into REO's log.",
      group="Log"),
]

# The 89 values Outbreak-ModernCam.exe 1.2.0 reads for File 1 (ro_camera_config.py).
EXE = [
    "input_device", "distance", "height", "shoulder", "shoulder_swap_time", "reveal_button", "reveal_time",
    "aim_strafe_speed", "look_drop", "pitch_trim", "fov_deg", "aim_button", "aim_blend_in", "aim_blend_out",
    "aim_blend", "aim_follow", "aim_follow_strength", "aim_free_look", "aim_snap_behind", "aim_snap_speed",
    "rotate_mode", "follow_strength", "follow_hold", "follow_dir", "yaw_offset", "free_look_return", "stick_speed",
    "invert_stick", "pitch_speed", "invert_pitch", "pitch_min", "pitch_max", "mouse_sensitivity", "recenter_button",
    "recenter_time", "swap_shoulder_button", "switch_view_button", "seat_hold_seconds", "cinematic_yield",
    "cinematic_yield_takes", "cinematic_yield_window", "cinematic_neutral_ms", "cinematic_move_confirm_ms",
    "special_button", "lens_button", "capture_button", "cam_collision", "cam_margin", "cam_block_top",
    "cam_ignore_props", "cam_recover", "cam_hold", "cam_snap", "near_clip", "far_clip", "fov_scale", "door_hold",
    "turn_to_camera", "own_turn", "turn_speed", "turn_deadzone", "sprint_button", "sprint_mode", "reload_button",
    "reload_animated", "shoot_button", "own_look_stick", "modern_aim", "aim_zero_deg", "aim_shoulder_min",
    "aim_sensitivity", "free_aim", "gun_pitch_max", "cut_match", "cut_match_max_cuts", "cut_match_radius",
    "char_light", "char_light_color", "crosshair", "crosshair_style", "crosshair_size", "crosshair_gap",
    "crosshair_thickness", "crosshair_color", "crosshair_dx", "crosshair_dy", "aim_pitch_lock",
    "modern_aim_deadzone", "debug_log",
]
