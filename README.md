# Resident Evil Outbreak — Modern Camera

An over-the-shoulder camera mod for **Resident Evil Outbreak Recompiled**, the unofficial PC port of Resident Evil
Outbreak (File #1, USA). It replaces the game's fixed camera angles with a camera behind your character that you turn
with the right stick or the mouse, with aiming, free gun elevation, a crosshair and its own settings window.

This mod is a recreation. I studied Snippy's original Outbreak ModernCam mod for Resident Evil Outbreak and learned how to recreate it in my own way for Resident Evil Outbreak Recompiled. Snippy gave me the idea of doing this for the recompiled version, and all credit for the original camera design goes to him. Go check out Snippy's ModernCam for the PS2 version: [heysnippy.com](https://heysnippy.com/mods/outbreak-moderncam/) and [the video](https://youtu.be/lpQUNS5klBY).

- Mod id `reo-modern-camera`, version **2.0.0**.
- 116 settings, every one of them changeable while you play.

> Unofficial fan project, not affiliated with or endorsed by Capcom or Snippy. See [Legal](#legal).

## Which disc it is for

**Resident Evil Outbreak (USA), SLUS-20765, disc version 2.00** - the release Resident Evil Outbreak Recompiled runs
(the mod's manifest: "Over-the-shoulder camera for the US release (SLUS-20765 v2.00)"). Every game address the camera
uses was found again on this disc.

- The original **Outbreak ModernCam is Snippy's mod for PCSX2**, made for the **Japanese version** (Biohazard Outbreak).
  This mod does not run on PCSX2 and is not Snippy's program; it is the same camera rebuilt for the US disc in the PC
  recompilation.
- Outbreak File #2 is not supported.

## Requirements

- **Resident Evil Outbreak Recompiled 1.5.0 or newer.**
- **Your own legal copy** of Resident Evil Outbreak (USA), SLUS-20765, disc version 2.00. This mod contains no disc
  image and no game files.

> **Status.** Resident Evil Outbreak Recompiled 1.5.0 prepares the game's code from your own disc on your PC. The
> camera can drive only in a prepared game that also has the camera's code sites compiled in (below): the program
> compiles them in when it prepares the game, so the camera works as soon as you switch the mod on.

The camera never writes the game's code. To stop the game's own camera and gun servo it switches two features compiled
into the prepared game code, `camera.freeze` and `camera.freeAim`, and it runs at three marks there (the game frame,
the pad mapper, and the per-character update for the reload). These code sites come with the download (`patches.json`,
made by `tools/gen_patches.py`), and the program checks every site against your own copy when it prepares your game.
Without them the camera cannot take over the game's camera: its status on the Mods tab and in the camera window says
so.

## Install

1. Download `reo-modern-camera-2.0.0.zip` from this repository's **Releases** page (it is also in the
   [`release`](release) folder). Do not unpack it.
2. In Resident Evil Outbreak Recompiled, open **Mods**, choose **Install Mods** and pick the zip.
3. Make sure the mod is switched on in the list. Press **F8** (or **Camera Window** next to Configure on the Mods tab)
   to open the camera window and tune it.

The Mods tab checks the whole archive before it writes anything and installs the mod into its own folder,
`mods\reo-modern-camera\`. To install by hand instead, unpack the zip into your mods folder so that
`mods\reo-modern-camera\mod.json` exists (the mods folder is `%APPDATA%\Resident Evil Outbreak Recompiled\mods\`, or
`mods\` next to the program in a portable copy).

**Updating:** install the new zip the same way. The Mods tab asks before it replaces the installed version, moves the
old one into `mods\.trash\` (nothing is deleted) and keeps your settings. Coming from the older v1 port: v1's saved
values override v2's defaults; use **Reset to defaults** once (in the camera window or on Configure) to start from
ModernCam's own File 1 defaults. It keeps a backup of the values it replaces.

## Features

- **Look**: the right stick, with separate yaw and pitch speeds, inversion and pitch limits; on keyboard and mouse,
  raw mouse look (the Free Cursor button, C, releases and recaptures the mouse).
- **Framing**: Main, Aim and Alt rigs, each with distance, left / right, height, field of view, look drop and pitch
  trim. An Aim or Alt value follows Main until you set it ("Custom"), as in ModernCam.
- **Keep Behind Character** (off by default): the camera swings back behind her; a free look eases back after the
  Recenter Speed. Camera Side and Yaw Offset put it elsewhere around her.
- **Modern Aim**: she turns to where the camera looks, the gun follows the camera up and down (Free Gun Elevation),
  Lock Look to the Gun keeps the view in the gun's range, and you can walk while aiming.
- **Crosshair** while aiming: dot or cross, size, gap, thickness, colour and offset, drawn by the program over the game
  picture (never part of the game's own picture).
- **Buttons**: Shoot fires with whatever button fires under your control type; Sprint (toggle or hold); Reload
  reloads the equipped weapon from carried ammo with the game's own animation; Recenter; Swap Shoulder (remembered);
  Alt View; Special Action; Reveal Camera (blends between the room's own camera and this one).
- **Right Stick Only Moves the Camera**: the right stick is Outbreak's ad-lib control, so the game gets a centred
  stick while the camera uses it (not on keyboard and mouse).
- **Collision** against the room's walls, with ModernCam's pull-in, hold and recover.
- **Room cuts**: Match Room Cuts borrows the room's own camera nearest your view, so the game draws what that view
  needs; Show Hidden Objects; Draw Distance; Character Light.
- **Doors and cutscenes**: the camera gives way when the game moves it, and can yield during cutscenes and come back
  when you move.

Default controls (ModernCam's File 1 set, all rebindable in the camera window's Controls tab): Aim = left trigger (on
keyboard and mouse: right mouse button), Shoot = right trigger, Reload = X / Square, Sprint = L3, Swap Shoulder = R3,
Free Cursor = C. **Steered By** chooses the device: Gamepad (Auto, the most active pad), Keyboard & Mouse, or one pad.

## The camera window

- Opens with **F8** - in the menus and over the running game - or with **Camera Window** on the Mods tab. F8 or Escape
  / Back closes it. It does not open over exclusive fullscreen (use Borderless or Windowed).
- Tabs as in ModernCam's window: **Camera**, **Look**, **Aiming**, **Controls**, then **Advanced** with every value
  ModernCam reads but hides. Every change reaches the running camera at the next game frame and is saved within a
  quarter of a second.
- The header has the camera's own **On / Off** (`enabled`), its status (Off, Waiting for a level, Driving the camera,
  Handed back to a cutscene, or that the game needs its code sites) and the credit line.
- **Reset to defaults** (press it twice within 3.5 s) puts back the mod's defaults and keeps the values it replaced in
  `mods\mod_config.reo-modern-camera.backup.json`.

## Switching it off

- The camera's own **On / Off** hands everything back to the game at once: the camera features switch off, and the
  room's fixed camera, the gun, the hide radii, the far plane, the lighting, any forced cut and the input overrides are
  the game's again.
- Switching the mod off on the Mods tab runs the camera once more with its own switch off (its hand-back) before it
  goes. If the camera ever stops on a fault, the program puts back what it changed in the game's data.
- One exception: a **Reload** is the game's own reload routine, called for you, as if you had reloaded in the game's
  menu. What it does (the rounds it loaded) is the game's and stays.

## What works and what does not

- Everything listed under Features works on the US v2.00 disc.
- **Watch Next Player** needs online play, which the program does not have yet: offline there is no one else to watch,
  so it does nothing (locked in the camera window).
- **Own Turning**, **Ignore Props**, **Move Confirm**, **Seat Detection Wait** and **Turn Deadzone** change nothing in
  File #1 (ModernCam uses them where it cannot set her heading, which File #1 never needs). They are kept for
  completeness and not offered in the camera window.
- **Draw Distance** goes from 0.1 to 1: on this disc ModernCam's values above 1 only enlarge the radii within which the
  game hides models close to the camera (which hid a partner standing near it), and there is no farther view to open.
- Outbreak File #2 and the Japanese discs are not supported.

## Settings

Defaults are ModernCam's own File 1 defaults. Bind codes are ModernCam's: `trig:L` / `trig:R` triggers, `pad:0x....`
controller buttons, `key:0x..` keys, `mb:0x..` mouse buttons.

### Camera

| Group | Setting (key) | Default | Range / choices | What it does |
|---|---|---|---|---|
| Rig: Main | Distance (`distance`) | 300 | 100 to 600 | How far behind her the camera sits. |
| Rig: Main | Left / Right (`shoulder`) | 50 | -150 to 150 | Sideways offset. Positive = over her right shoulder, negative = left. Swap Shoulder writes the new side here. |
| Rig: Main | Height (`height`) | 130 | 50 to 250 | Raises the camera and the point it looks at together, so the view moves up or down without tilting. |
| Rig: Main | Field of View (`fov_deg`) | 60 | 0 to 90 | Vertical field of view in degrees (the exe: 30-90). 0 keeps each room's own FOV. |
| Rig: Aim | Aim: Custom Distance (`aim_distance_set`) | Off | On / Off | On: the aim camera (blended in while aiming) uses its own distance below. Off: it follows the main camera's. |
| Rig: Aim | Aim: Distance (`aim_distance`) | 300 | 100 to 600 | How far behind her the camera sits. Applies while Aim: Custom Distance is on. |
| Rig: Aim | Aim: Custom Left / Right (`aim_shoulder_set`) | Off | On / Off | On: the aim camera (blended in while aiming) uses its own left / right below. Off: it follows the main camera's. |
| Rig: Aim | Aim: Left / Right (`aim_shoulder`) | 50 | -150 to 150 | Sideways offset, + = her right shoulder. Applies while Aim: Custom Left / Right is on. |
| Rig: Aim | Aim: Custom Height (`aim_height_set`) | Off | On / Off | On: the aim camera (blended in while aiming) uses its own height below. Off: it follows the main camera's. |
| Rig: Aim | Aim: Height (`aim_height`) | 130 | 50 to 250 | Lifts the camera and its look point together. Applies while Aim: Custom Height is on. |
| Rig: Aim | Aim: Custom Field of View (`aim_fov_deg_set`) | Off | On / Off | On: the aim camera (blended in while aiming) uses its own field of view below. Off: it follows the main camera's. |
| Rig: Aim | Aim: Field of View (`aim_fov_deg`) | 60 | 0 to 90 | Vertical field of view. 0 keeps the room's own FOV. Applies while Aim: Custom Field of View is on. |
| Rig: Alt | Alt: Custom Distance (`alt_distance_set`) | Off | On / Off | On: the Alt View camera uses its own distance below. Off: it follows the main camera's. |
| Rig: Alt | Alt: Distance (`alt_distance`) | 300 | 100 to 600 | How far behind her the camera sits. Applies while Alt: Custom Distance is on. |
| Rig: Alt | Alt: Custom Left / Right (`alt_shoulder_set`) | Off | On / Off | On: the Alt View camera uses its own left / right below. Off: it follows the main camera's. |
| Rig: Alt | Alt: Left / Right (`alt_shoulder`) | 50 | -150 to 150 | Sideways offset, + = her right shoulder. Applies while Alt: Custom Left / Right is on. |
| Rig: Alt | Alt: Custom Height (`alt_height_set`) | Off | On / Off | On: the Alt View camera uses its own height below. Off: it follows the main camera's. |
| Rig: Alt | Alt: Height (`alt_height`) | 130 | 50 to 250 | Lifts the camera and its look point together. Applies while Alt: Custom Height is on. |
| Rig: Alt | Alt: Custom Field of View (`alt_fov_deg_set`) | Off | On / Off | On: the Alt View camera uses its own field of view below. Off: it follows the main camera's. |
| Rig: Alt | Alt: Field of View (`alt_fov_deg`) | 60 | 0 to 90 | Vertical field of view. 0 keeps the room's own FOV. Applies while Alt: Custom Field of View is on. |
| Following | Keep Behind Character (`rotate_mode`) | Off | On / Off | On (the exe's 'follow'): the camera swings back behind her as she moves. Off ('manual', ModernCam's File 1 default): it stays wherever you leave it. |
| Following | Follow Speed (`follow_strength`) | 0.12 | 0.02 to 0.4 | How quickly the camera swings behind her (Keep Behind Character on). |
| Following | Recenter Speed (`free_look_return`) | 1.2 | 0 to 4 | Seconds for a look with the right stick or mouse to ease back behind her (Keep Behind Character on). 0 never releases it. |

### Look

| Group | Setting (key) | Default | Range / choices | What it does |
|---|---|---|---|---|
| Look | Sensitivity (gamepad) (`stick_speed`) | 140 | 40 to 300 | Turn speed in degrees per second at full right-stick push. |
| Look | Sensitivity (keyboard & mouse) (`mouse_sensitivity`) | 0.15 | 0.02 to 0.6 | Degrees per mouse count x 0.4, as the exe. Raw mouse motion, no acceleration. |
| Look | Invert Up/Down (`invert_pitch`) | Off | On / Off |  |

### Aiming

| Group | Setting (key) | Default | Range / choices | What it does |
|---|---|---|---|---|
| Aiming | Lock Look to the Gun (`aim_pitch_lock`) | On | On / Off | While aiming, stop the camera tilting further than the gun can follow. |
| Aiming | Aim Sensitivity (`aim_sensitivity`) | 0.55 | 0.1 to 1.5 | Look speed multiplier while aiming. |
| Aiming | Gun Elevation (`gun_pitch_max`) | 45 | 5 to 45 | How far up and down the gun can point, in degrees. |
| Reticle | Crosshair (`crosshair`) | On | On / Off | Show a crosshair while aiming (drawn by REO over the game picture). |
| Reticle | Crosshair Shape (`crosshair_style`) | Dot | Dot, Cross |  |
| Reticle | Crosshair Size (`crosshair_size`) | 3 | 1 to 16 | Pixels: the dot's radius or the cross's arm length. |
| Reticle | Crosshair Gap (`crosshair_gap`) | 3 | 0 to 16 | Pixels between the centre and each arm (Cross only). |
| Reticle | Crosshair Thickness (`crosshair_thickness`) | 2 | 1 to 6 | Pixels (Cross only). |

### Controls

| Group | Setting (key) | Default | Range / choices | What it does |
|---|---|---|---|---|
| Input | Steered By (`input_device`) | `auto` |  | auto (Gamepad: the most active pad), kbm (Keyboard & Mouse) or pad:N (that pad). The camera's own input; the game keeps REO's controls. |
| Binds | Aim (`aim_button`) | `trig:L` = left trigger |  | Hold to aim (ModernCam's File 1 bind: left trigger). With Modern Aim the camera holds R1, the game's aim, for her. On Keyboard & Mouse a pad or trigger bind is the right mouse button. |
| Binds | Shoot (`shoot_button`) | `trig:R` = right trigger |  | Fires with whichever button fires under your control type (File 1 bind: right trigger). |
| Binds | Reload (`reload_button`) | `pad:0x4000` = X (Xbox) / Square (PlayStation) |  | Reloads the equipped weapon from carried ammo with the game's own reload animation (File 1 bind: X / Square). |
| Binds | Sprint (toggle) (`sprint_button`) | `pad:0x0040` = L3 (left stick click) |  | Sprint without holding the dash button; takes the dash action off its original button (File 1 bind: L3). |
| Binds | Free Cursor (`capture_button`) | `key:0x43` = C key |  | Keyboard & Mouse: frees or captures the mouse (C). |
| Binds | Recenter (`recenter_button`) | (none) |  | Snaps the camera behind her. |
| Binds | Swap Shoulder (`swap_shoulder_button`) | `pad:0x0080` = R3 (right stick click) |  | Moves the camera to her other shoulder and saves the side (File 1 bind: R3). |
| Binds | Special Action (`special_button`) | (none) |  | While held, presses the dash action's button for the game (the exe's note: for characters whose special move shares the dash button), and keeps the bound button from the game. |
| Binds | Watch Next Player (`lens_button`) | (none) |  | Multiplayer only: the camera follows the next human player while you keep control. Offline there is no one else to watch, so it does nothing (online play is not available). **Locked in the camera window.** |
| Binds | Alt View (`switch_view_button`) | (none) |  | Toggles the Alt rig. |

### Advanced (Advanced: the values the exe reads but hides)

| Group | Setting (key) | Default | Range / choices | What it does |
|---|---|---|---|---|
| Framing tilt: Main | Look Drop (`look_drop`) | 0 | -150 to 150 | How far below its own height the camera looks: a fixed downward framing tilt (the exe keeps it out of its window). |
| Framing tilt: Main | Pitch Trim (`pitch_trim`) | 0 | -45 to 45 | Constant up/down tilt added to the camera's orbit pitch. |
| Framing tilt: Aim | Aim: Custom Look Drop (`aim_look_drop_set`) | Off | On / Off | On: the aim camera (blended in while aiming) uses its own look drop below. Off: it follows the main camera's. |
| Framing tilt: Aim | Aim: Look Drop (`aim_look_drop`) | 0 | -150 to 150 | How far below its own height the camera looks. Applies while Aim: Custom Look Drop is on. |
| Framing tilt: Aim | Aim: Custom Pitch Trim (`aim_pitch_trim_set`) | Off | On / Off | On: the aim camera (blended in while aiming) uses its own pitch trim below. Off: it follows the main camera's. |
| Framing tilt: Aim | Aim: Pitch Trim (`aim_pitch_trim`) | 0 | -45 to 45 | Constant tilt added to the orbit pitch. Applies while Aim: Custom Pitch Trim is on. |
| Framing tilt: Alt | Alt: Custom Look Drop (`alt_look_drop_set`) | Off | On / Off | On: the Alt View camera uses its own look drop below. Off: it follows the main camera's. |
| Framing tilt: Alt | Alt: Look Drop (`alt_look_drop`) | 0 | -150 to 150 | How far below its own height the camera looks. Applies while Alt: Custom Look Drop is on. |
| Framing tilt: Alt | Alt: Custom Pitch Trim (`alt_pitch_trim_set`) | Off | On / Off | On: the Alt View camera uses its own pitch trim below. Off: it follows the main camera's. |
| Framing tilt: Alt | Alt: Pitch Trim (`alt_pitch_trim`) | 0 | -45 to 45 | Constant tilt added to the orbit pitch. Applies while Alt: Custom Pitch Trim is on. |
| Look | Vertical Sensitivity (`pitch_speed`) | 90 | 20 to 300 | Up/down speed in degrees per second at full right-stick push. |
| Look | Invert Left/Right (`invert_stick`) | Off | On / Off |  |
| Look | Lowest Look Angle (`pitch_min`) | -35 | -89 to 0 |  |
| Look | Highest Look Angle (`pitch_max`) | 70 | 0 to 89 |  |
| Aim tuning | Aim Zoom Speed (fallback) (`aim_blend`) | 0.25 | 0.02 to 1 | Used for zooming in or out when that speed below is 0 (the exe: when it is not set). |
| Aim tuning | Aim Zoom In Speed (`aim_blend_in`) | 0.35 | 0 to 1 | 0 uses the fallback above. |
| Aim tuning | Aim Zoom Out Speed (`aim_blend_out`) | 0.15 | 0 to 1 | 0 uses the fallback above. |
| Aim tuning | Follow While Aiming (`aim_follow`) | On | On / Off | Used when Modern Aim is off. |
| Aim tuning | Follow Speed While Aiming (`aim_follow_strength`) | 0.3 | 0.02 to 1 |  |
| Aim tuning | Free Look While Aiming (`aim_free_look`) | On | On / Off |  |
| Aim tuning | Snap Behind on Aim (`aim_snap_behind`) | On | On / Off | Pressing aim swings the camera straight behind her (Modern Aim off). |
| Aim tuning | Snap Speed (`aim_snap_speed`) | 0.5 | 0.1 to 1 |  |
| Aim tuning | Gun Zero (`aim_zero_deg`) | -0.1 | -45 to 45 | Gun elevation when the camera is level. |
| Aim tuning | Aim Shoulder Minimum (`aim_shoulder_min`) | 0 | 0 to 150 | Minimum sideways offset while aiming. |
| Aim tuning | Aim Turn Deadzone (`modern_aim_deadzone`) | 12 | 0 to 45 | While aiming, how many degrees the camera and her facing may differ before she turns. |
| Aim tuning | Modern Aim (`modern_aim`) | On | On / Off | Aim where the camera looks: she turns with the camera while aiming and the gun follows it. |
| Aim tuning | Free Gun Elevation (`free_aim`) | On | On / Off | The gun follows the camera up and down instead of snapping to the game's three angles. |
| Aim tuning | Strafe Speed While Aiming (`aim_strafe_speed`) | 45 | 0 to 120 | File 1 roots her when she aims; the camera moves her with the left stick at this speed. 0 turns it off. |
| Turning | Turn to Camera (`turn_to_camera`) | Off | Off, Moving, Always | Moving: she faces the direction you push relative to the camera. Always: she always faces where the camera looks. |
| Turning | Turn Speed (`turn_speed`) | 165 | 30 to 720 | Degrees per second when the camera turns her. |
| Turning | Turn Deadzone (`turn_deadzone`) | 10 | 1 to 45 | **Changes nothing in File #1; not offered in the camera window.** |
| Turning | Own Turning (`own_turn`) | Off | On / Off | File 2 only in the exe (its heading freeze); File 1 has none, so this changes nothing - as in the exe. **Changes nothing in File #1; not offered in the camera window.** |
| Following | Follow Hold (`follow_hold`) | 0.18 | 0 to 1 | Seconds she counts as still moving after her last step (the follow heading). |
| Following | Camera Side (`follow_dir`) | Behind her | Behind her, In front of her | Where Keep Behind, snap and recenter put the camera (the exe's follow_dir 1 / -1). |
| Following | Yaw Offset (`yaw_offset`) | 0 | -180 to 180 | Degrees added to the follow and snap direction. |
| Following | Shoulder Swap Speed (`shoulder_swap_time`) | 0.18 | 0.02 to 1 | How quickly the camera slides to the other shoulder. |
| Following | Recenter Time (`recenter_time`) | 0.25 | 0 to 2 | Seconds the Recenter button eases the camera back. |
| Collision | Camera Collision (`cam_collision`) | On | On / Off | Pulls the camera in front of walls. |
| Collision | Wall Margin (`cam_margin`) | 55 | 0 to 150 | How far in front of a wall the camera stops. |
| Collision | Low Obstacle Height (`cam_block_top`) | 25 | 0 to 150 | Walls whose top is below the look point plus this do not pull the camera in. |
| Collision | Ignore Props (`cam_ignore_props`) | On | On / Off | Skip walls flagged as props. File 1's wall data has no prop flag, so this changes nothing - as in the exe. **Changes nothing in File #1; not offered in the camera window.** |
| Collision | Collision Recover Speed (`cam_recover`) | 0.1 | 0.02 to 1 |  |
| Collision | Collision Hold (`cam_hold`) | 0.12 | 0 to 1 | Seconds the camera stays pulled in after a wall clears. |
| Collision | Collision Ease In (`cam_snap`) | 0 | 0 to 1 | 0 pulls in instantly; higher values ease in. |
| Projection | Near Value (`near_clip`) | 50 | 0 to 500 | The exe's near_clip, written to the camera block (0 keeps the game's). On this disc that word is the fixed camera's look distance, so it has no visible effect while the camera drives. |
| Projection | Far Clip (`far_clip`) | 0 | 0 to 200000 | The projection's far plane. 0 keeps the game's (50,000). Larger values show nothing more: the game's scenes, sky included, lie within 50,000. Smaller values cut off distant scenery. |
| Projection | Horizontal Scale (`fov_scale`) | 0 | 0 to 3 | The projection's horizontal scale (the game uses 1.33). 0 keeps the game's, and REO's widescreen setting. |
| World | Match Room Cuts (`cut_match`) | On | On / Off | Borrows the room's fixed camera nearest the view, so the game draws what that angle needs. |
| World | Cut Match Radius (`cut_match_radius`) | 250 | 50 to 1000 |  |
| World | Cut Match Max Cuts (`cut_match_max_cuts`) | 6 | 1 to 64 | Rooms with more cuts than this are not matched. |
| World | Show Hidden Objects (`show_hidden_objects`) | Off | On / Off | Keeps objects the fixed camera would hide visible. |
| World | Draw Distance (`draw_distance`) | 1.0 | 0.1 to 1 | Below 1: shrinks the radii within which the game hides models close to the camera, as the exe. It stops at 1: on this disc the exe's values above 1 only enlarge those radii, which hides nearby characters, and there is no farther view to open (the game's scenes, sky included, lie within its far plane). |
| World | Character Light Override (`char_light`) | Off | On / Off |  |
| World | Character Light Colour (`char_light_color`) | `#808080` |  |  |
| Doors and cutscenes | Hold Camera Through Doors (`door_hold`) | On | On / Off | When the game moves the camera (doors, scripted shots), take it back. |
| Doors and cutscenes | Yield During Cutscenes (`cinematic_yield`) | Off | On / Off | Hand the camera back while the game runs its own cinematic instead of fighting it; returns when you move again. |
| Doors and cutscenes | Cutscene Detection Takes (`cinematic_yield_takes`) | 3 | 2 to 10 |  |
| Doors and cutscenes | Cutscene Detection Window (`cinematic_yield_window`) | 1.0 | 0.2 to 5 | Seconds. |
| Doors and cutscenes | Resume Delay (ms) (`cinematic_neutral_ms`) | 400 | 50 to 2000 | How long the stick must rest before moving resumes the camera. |
| Doors and cutscenes | Move Confirm (ms) (`cinematic_move_confirm_ms`) | 180 | 0 to 2000 | A dormant value of the exe: it is defined but never read, so it changes nothing. **Changes nothing in File #1; not offered in the camera window.** |
| Input | Sprint Mode (`sprint_mode`) | Toggle | Toggle, Hold | Toggle stops when you stop moving. |
| Input | Right Stick Only Moves the Camera (`own_look_stick`) | On | On / Off | The right stick is the game's ad-lib control. On, the game sees it centred, so looking around does not trigger ad-libs (not on Keyboard & Mouse, as the exe). |
| Input | Seat Detection Wait (`seat_hold_seconds`) | 4 | 0 to 30 | The exe's online seat detection (which character is yours). REO knows the local player, so this changes nothing. **Changes nothing in File #1; not offered in the camera window.** |
| Reload | Animated Reload (`reload_animated`) | On | On / Off | The reload uses the game's own combine routine and animation. Off: no reload (this exe version has no other reload). |
| Reticle | Crosshair Colour (`crosshair_color`) | `#f2f2f2` |  |  |
| Reticle | Crosshair Offset X (`crosshair_dx`) | 0 | -200 to 200 | Pixels, + = right. |
| Reticle | Crosshair Offset Y (`crosshair_dy`) | 0 | -200 to 200 | Pixels, - = up. |
| Reveal camera | Reveal Camera (`reveal_button`) | (none) |  | A capture aid hidden in the exe: blends between the room's own fixed camera and this one. |
| Reveal camera | Reveal Speed (`reveal_time`) | 1.6 | 0.2 to 6 | Seconds for the reveal blend. |
| Log | Camera Event Log (`debug_log`) | Off | On / Off | Write the camera's events ([evt] lines) into REO's log. |


## Building from source

The release zip holds the built mod; building it yourself is only needed to change it. You need clang with the MIPS
target (LLVM 18.1.8 is the last official Windows release that has it), `ld.lld`, and `RecompModTool` from
[N64Recomp](https://github.com/N64Recomp/N64Recomp). From Git Bash, in this folder:

```bash
mingw32-make OS=Linux CC=<path to clang.exe> LD=<path to ld.lld.exe> RECOMP_MOD_TOOL=<path to RecompModTool.exe>
```

- The output is `build/REO_ModernCam.nrm`. To try a build, use **Mods → Install Mods** and pick this folder's
  `mod.json`, or pack `mod.json`, `icon.png`, `window_layout.json` and the `.nrm` into a zip as in the release.
- The settings are defined once, in `tools/settings.py`; `python tools/gen_manifests.py` regenerates `mod.toml`,
  `mod.json`, `manifest.json` and `window_layout.json` from it.
- `python tools/gen_patches.py` writes `patches.json`, the camera's code sites, from your own disc files (set
  `REO_GAME_DIR` to the folder with the files extracted from your disc; the script stops with a message when it is not
  set). The file holds addresses and instruction words of your copy of the game: keep it to yourself (the
  `.gitignore` keeps it out of the repository).
- `python tests/test_driver.py` runs the compiled `build/mod.elf` in a small MIPS interpreter against your disc's code
  and data (run `make` first) with a stand-in for the program's host functions (set `REO_GAME_DIR`, and `LLVM_OBJDUMP`
  to your `llvm-objdump.exe` unless `llvm-objdump` is on your `PATH`).
- [HOST_API.md](HOST_API.md) is the contract between the camera and the program's host services.

## Credits

- **Snippy** ([heysnippy.com](https://heysnippy.com/mods/outbreak-moderncam/)) - Outbreak ModernCam, the original
  camera design. This mod is a recreation. I studied Snippy's original Outbreak ModernCam mod for Resident Evil Outbreak and learned how to recreate it in my own way for Resident Evil Outbreak Recompiled. Snippy gave me the idea of doing this for the recompiled version, and all credit for the original camera design goes to him. Go check out Snippy's ModernCam for the PS2 version: [heysnippy.com](https://heysnippy.com/mods/outbreak-moderncam/) and [the video](https://youtu.be/lpQUNS5klBY).
- **Capcom** - Resident Evil Outbreak, the original game.
- This mod was built by RaccoonRecomp with AI-assisted programming (Claude, Anthropic).

## Legal

- This is an **unofficial fan project**. It is **not affiliated with, endorsed or sponsored by Capcom**, and it is not
  an official release of Snippy's Outbreak ModernCam.
- *Resident Evil*, *Resident Evil Outbreak* and all related names, characters and content are trademarks and property
  of Capcom. **All copyright and credit for the game belong to Capcom.** The names are used only to say which game this
  mod works with.
- **You need your own legal copy of the game.** This mod contains **no disc image and no game files**. Its sources name
  the game addresses the camera uses, and its patch generator (`tools/gen_patches.py`) holds the original instruction
  words of the code sites it checks against your own disc.
