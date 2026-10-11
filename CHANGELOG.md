# Changelog

## 2.1.0

- **The camera stays on your character**: it no longer shifts by itself. It turns only when you turn it (right stick /
  mouse, Recenter) and, with Keep Behind Character, with her facing while you move her (and back after a free look, as
  before) - a knockback, a hit, the recoil of a shot or the game's auto-aim turning or pushing her no longer swing it.
  It keeps its shoulder side (only Swap Shoulder changes it) and it does not pull back: a wall can still bring it
  closer, and it goes back to its distance when you move her or the camera, never further and never by itself.
- **Lock On Enemy** (new setting, Aiming tab, on by default): while she aims and the game has locked onto an enemy (its
  auto-aim: the target it chose when she raised the weapon, held while the aim button is held, also through a hit),
  the camera turns onto that enemy and stays on it as it moves; Modern Aim turns her to the enemy, not away from it;
  the camera keeps the shoulder side it had when the lock began (Swap Shoulder does nothing until the lock ends) and
  does not pull back (walls can still pull it in). The lock ends when the aim is let go or the game can no longer
  target that enemy; the camera then turns back behind her, on the same side and at the same distance. Off: no enemy
  follow; while she aims the camera stays on her as everywhere else and Modern Aim works as in 2.0.0.
- Why: in 2.0.0, with Keep Behind Character, the camera followed the direction she was moving in, so knockbacks, hits
  and recoil swung it around (also while she aimed with the game's own aim button), and after a wall pulled it in it
  pulled back out during the fight; Modern Aim turned her from the enemy the game had locked onto toward wherever the
  camera looked, and the camera never followed the enemy.
- **Walls and pictures stay**: what one of the room's fixed cameras leaves out (the wall it looks through, the
  pictures on that wall, a light next to its lens) stays drawn while the camera drives, also at a cut change. Before,
  the game still picked its fixed camera from her position and hid those objects from the free view: J's Bar's
  restroom showed a black far wall without its framed poster, the Jack's Bar sign lost its glowing frame, The Hive's
  corridors lost their ceilings. What every fixed camera that looks at an object leaves out stays out (the restroom's
  vent stays open, its grille on the floor), and so do objects no fixed camera draws and what the game's events hide.
  No setting; Show Hidden Objects is unchanged.
- **J's Bar's restroom mirrors show the room reflected**: the game's mirrors are a mirrored copy of the room's door end
  built behind the glass for its own fixed camera only; from the camera's angles they showed black glass and two
  floating posters. While the camera drives there, the room is drawn a second time, mirrored about the glass and
  clipped to it, with the game's own functions, before the room's own objects, so the glass blends over the reflection
  as over the copy: from the game's fixed camera it looks as the game's own mirror (measured: within 2.4 of 255 per
  channel inside the glass), from every other angle it is the room as a mirror shows it. The copy is not drawn then.
  Characters are not reflected (the game's mirror never reflected them). It needs a game build with the new mark
  `camera.roomDraw` of `patches.json` (and HOST_API.md contract 2); without it nothing changes. No setting.
- **The restroom's light bar**: its glow card (object 75), which only the fixed camera's angle shows as a glow, is not
  drawn while the camera drives there; from the camera's angles it was a board over the bulbs.
- One setting added (Lock On Enemy, 117 settings). `patches.json` has one site more: the mark `camera.roomDraw`
  (submain.bin), for the mirrors.

## 2.0.0

First release on GitHub: the mod as a downloadable zip for **Mods → Install Mods** (`release/reo-modern-camera-2.0.0.zip`).
The mod's code and manifest are unchanged.

- **Never touches the game's code**: the camera freeze and free aim are code sites compiled into the game's code
  (`patches.json`, made from your own disc by `tools/gen_patches.py`) and switched by the camera (`camera.freeze`,
  `camera.freeAim`). v1 wrote the patch words into the game's code, which took that code out of native execution.
- **A narrower freeze** of the game's camera handlers 2 and 3: only their camera writes are skipped, their rail
  bookkeeping keeps running, so the game's camera state is current when the camera hands back.
- **ModernCam's own input**: the camera's own device and binds through the program (gamepad or keyboard and mouse,
  device choice), applied where the game maps the pad, as ModernCam did. ModernCam's File 1 binds, all rebindable:
  Aim = left trigger, Shoot = right trigger, Sprint = L3, Swap Shoulder = R3, Reload = X / Square, Free Cursor = C.
- **ModernCam's own File 1 defaults** (distance 300, shoulder 50, height 130, field of view 60, ...).
- **Added from ModernCam**: mouse look and sensitivity, device choice, free cursor, the crosshair (drawn by the
  program), reload, special action, reveal camera, watch next player (online only), Aim / Alt rigs value by value with
  "Custom" switches, camera side, follow hold, yaw offset, recenter time, shoulder swap time, aim blend, character
  light colour, Swap Shoulder remembering the side, the event log, and ModernCam's dormant values on the Advanced tab.
- **Draw Distance** goes from 0.1 to 1: on this disc larger values only hide models near the camera.
- **116 settings** (76 in v1): all 89 of ModernCam's, plus the camera's own switch, the 12 Aim / Alt values with their
  12 "Custom" switches, Draw Distance and Show Hidden Objects.
- **The camera window** (`window_layout.json`): ModernCam's tabs Camera, Look, Aiming and Controls, then Advanced.

## 1.0.0

- The first port of Outbreak ModernCam to the US disc (SLUS-20765 v2.00): the over-the-shoulder camera with the right
  stick, Main, Aim and Alt framings, free gun elevation and walking while aiming, camera collision, room-cut matching,
  and optional sprint and shoot buttons, shoulder swap and recenter.
