# Changelog

## 2.0.1

- **Aim lock**: while she aims and the game has locked onto an enemy (its auto-aim: the target it chose when she raised
  the weapon, held while the aim button is held, also through a hit), the camera turns onto that enemy and stays on it
  as it moves; Modern Aim turns her to the enemy, not away from it; the camera keeps the shoulder side it had when the
  lock began (Swap Shoulder does nothing until the lock ends) and does not pull back (walls can still pull it in). The
  lock ends when the aim is let go or the game can no longer target that enemy; then the camera behaves as before.
- Why: in 2.0.0 Modern Aim turned her from the enemy the game had locked onto toward wherever the camera looked
  (unless the camera already pointed within the Aim Turn Deadzone of it), the camera never followed the enemy, with
  Keep Behind Character it swung after her steps while she aimed with the game's own aim button, and after a wall
  pulled it in it pulled back out during the fight.
- No setting is added or changed. `patches.json` is unchanged.

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
