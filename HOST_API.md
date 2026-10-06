# REO_ModernCam v2: the host API it imports

This is the contract between the Modern Camera mod (`build/mod.elf` / `REO_ModernCam.nrm`, v2.0.0) and the parts of
Resident Evil Outbreak Recompiled that run it: the sandboxed mod runtime, the camera's host services and the live
settings window. The mod side of the same contract is `include/reo_host.h`. The fake host in `tests/test_driver.py`
implements every function below and is the reference for its behaviour.

Contract version 1. Everything here is for File #1, USA v2.00 (SLUS-20765).

## 1. How the mod runs

- **Code.** `mod.elf` is big-endian MIPS II (O32) with COP1 single and double precision. It is linked at `0x81000000`
  and needs 1 MB of mod memory. The runtime uses N64Recomp's memory model: words are stored natively, a byte at
  address `A` is at host byte `A ^ 3`, and a halfword at `A ^ 2`.
- **Guest memory.** An aligned word access to `0x00000000`-`0x01FFFFFF` is EE RAM, read and written through
  `kernel::GuestAccess`. The mod only makes aligned 32-bit accesses there: it shifts bytes and halfwords in and out
  of the word itself (`include/mcam_mem.h`).
  - The mod never writes game code. A refused write is a mod bug: stop the mod and report it.
  - Reads: the mod reads only data. The single exception is the button-map table at `0x002343C0`, which lies inside
    the executable's one loadable segment.
- **Budget.** Measured in the port's simulator over real game RAM:
  - J's Bar, 143 walls: median 9,800 and at most 38,800 instructions per game frame. The maximum comes from the
    wall-cache refresh, once a second.
  - The Hive corridor: median 8,000, at most 26,400.
  - Synthetic tests: median 3,400.

  A per-call limit of 2,000,000 instructions is ample. On overrun, stop the mod and show a diagnostic.
- **Imports.** Everything listed in section 3 is imported from `"*"`. Resolve them by name when the mod loads. A
  missing import means the mod cannot run: report it and do not load the mod.
- **ABI.** Every argument is one 32-bit word in `$a0`-`$a3`: a pointer into mod memory, an integer, or the IEEE-754
  bits of an `f32`. Every result is one word in `$v0`.
  - The one exception is N64Recomp's `recomp_get_config_double`, which returns a double in `$f0`/`$f1`.
  - A structure passed by pointer is made of 32-bit words only. The host reads and writes it word by word.
  - A string is a C string in mod memory. Read it with the runtime's byte rule.

## 2. The events the mod exports

| Event | Where the runtime calls it | Arguments |
|---|---|---|
| `recomp_on_play_main(void)` | once per game frame, at the game-frame mark: exe `0x0019F6C0`, `patches.json` site `camera.gameFrame`, hook `game.frame` (the rate profile's frame mark, shared) | none |
| `recomp_on_pad_map(u32 heldWordAddr, u32 charRecord)` | at the `camera.padMap` mark, exe `0x001B0C5C`, before the mapper's `lw t4,0(a2)`; once per character mapped in a frame | `heldWordAddr` = `$a2` (the mapped record's `+0x08` word, `0x00324248` for the local player offline); `charRecord` = `$s0` (the character record, `0x004A5C30 + 0x10E0 * slot`) |

**Order within a game frame.**
1. The frame mark: `recomp_on_play_main` computes the camera. It writes the eye, look-at, FOV and data values, and
   decides that frame's pad transform.
2. The game's character loop calls the mapper (game.bin `0x0066A0D0`), and the pad-map mark fires:
   `recomp_on_pad_map` applies the transform to her record only.
3. The per-character update's entry (`camera.reload`, game.bin `0x0066B500`) runs any queued guest call (4.10).
4. The camera dispatcher runs its handlers, whose camera writes are switched off (section 5).

If a host does not raise `recomp_on_pad_map`, the mod falls back to v1's once-per-frame write after 30 frames. That
write can race the game's pad read, and the mod logs it.

## 3. The imports

| Function | Section |
|---|---|
| `u32 recomp_get_config_u32(const char* key)`, `double recomp_get_config_double(const char* key)` | 4.1 |
| `u32 reo_feature_set(const char* name, u32 on)`, `u32 reo_feature_state(const char* name)` | 4.2 |
| `u32 reo_image_loaded(const char* image)` | 4.2 |
| `u32 reo_vblank_rate_mhz(void)` | 4.3 |
| `u32 reo_config_get_rgb(const char* key)`, `void reo_config_set_float(const char* key, u32 floatBits)` | 4.4 |
| `u32 reo_cam_input(ReoCamInput* out)` | 4.5 |
| `void reo_cam_input_hide(u32 actions)` | 4.6 |
| `void reo_pad_override(u32 flags)` | 4.7 |
| `void reo_mouse_capture(u32 on)` | 4.8 |
| `void reo_cam_crosshair(const ReoCrosshair* c)` | 4.9 |
| `u32 reo_guest_call_at(const char* hook, const ReoGuestCall* call)`, `u32 reo_guest_call_result(u32 ticket, u32* v0)` | 4.10 |
| `u32 reo_local_player_slot(void)`, `u32 reo_human_player_slots(void)` | 4.11 |
| `void reo_cam_status(u32 status, u32 detail)` | 4.12 |
| `void reo_log(const char* text)` | 4.13 |

All of them are called on the game thread, from inside the two events.

## 4. The functions

### 4.1 Settings (N64Recomp's config API)
The mod reads every setting every frame, so the window's changes apply at the next game frame, as the exe re-read
`ro_settings.json`. The keys and types are those of `mod.toml` and `mod.json` (116 settings, generated from
`tools/settings.py`).

| Setting type (`mod.json`) | `recomp_get_config_u32` | `recomp_get_config_double` |
|---|---|---|
| `boolean` | 0 / 1 | - |
| `choice` | the index of the choice | - |
| `integer` | the value, truncated | the value (the mod reads the crosshair offsets this way, because they can be negative) |
| `number` | the value, truncated | the value |
| `string` | not read by the mod | - |

The string settings are read by the host, not by the mod:
- the binds (4.5);
- `input_device` (4.5);
- the two colours, through `reo_config_get_rgb` (4.4).

### 4.2 Feature switches
`reo_feature_set(name, on)` asks for a feature of the game's feature profile, on or off, through
`kernel::FeatureSwitches::Set`. It returns the state as the host knows it at the call:

| Value | State |
|---|---|
| 0 | Off |
| 1 | Active |
| 2 | Waiting (its overlay is not loaded) |
| 3 | Unavailable (the game code was built without its sites, or a word differs) |
| 4 | No feature of that name |

`reo_feature_state(name)` returns the same without a request.

The mod uses two features, both from `patches.json` (section 5):
- `camera.freeze`: on while the camera drives a live level; off when it hands back, in cutscene yield, and while no
  level is live.
- `camera.freeAim`: on while the settings Modern Aim and Free Gun Elevation are both on.

A request takes effect at the next vblank start. The mod only sends a request when its wish changes.

The mod treats Unavailable or Unknown for `camera.freeze` as "this game build cannot run the camera":
- it hands everything back;
- it reports status 4;
- it does not drive, because the game's handlers would fight it.

`reo_image_loaded("game.bin")` returns 1 while the code of the profile image `game.bin` (xxh64 `1b4bcbcfd81bd910`) is
in memory. The mod counts a level as live only then, and only when the player's record holds a position. This
replaces v1's check of the patch words, since the mod no longer reads code.

### 4.3 `reo_vblank_rate_mhz`
The field rate in millihertz: 59940, or 60000 with the owner's `FieldRate = 60hz`. The mod's `dt` is the game's own
vblank counter (`0x003243F4`) divided by this rate. That makes the camera frame-rate independent, and the same at
30 and 60 fps.

### 4.4 Colours and saved values
- `reo_config_get_rgb(key)` parses a colour setting as ModernCam's `_hex_rgb` does:
  - it accepts `"#rgb"` or `"#rrggbb"`, with the `'#'` optional;
  - it returns `0x00RRGGBB`, or `0xFFFFFFFF` when the value is not a colour (the mod then uses the exe's default).
  - The keys are `char_light_color` and `crosshair_color`.
- `reo_config_set_float(key, bits)` stores a number setting as the value the window and `mod_config.json` show.
  - Swap Shoulder calls it with `"shoulder"` and the new signed magnitude, as the exe's `update_settings`.
  - It must update the snapshot `recomp_get_config_*` reads, notify the window, and save (debounced, about 250 ms,
    like the exe's `flushWrites`).

### 4.5 `reo_cam_input`: the camera's own device
This fills `ReoCamInput` (12 words) for the current game frame. It returns 1 when valid, or 0 when the host has no
camera input service. On 0, the mod reads look and movement from the game's own pad frames and has no binds.

| Word | Field | Meaning |
|---|---|---|
| 0 | `version` | 1 |
| 1 | `device` | 0 none, 1 gamepad, 2 keyboard & mouse. From `input_device`: `"kbm"` → 2; `"auto"` → 1 (the most active pad, as the exe's `_pad_activity`); `"pad:N"` → 1 (XInput or SDL pad N). |
| 2 | `held` | camera actions held now (bits below) |
| 3 | `bound` | camera actions whose bind setting is not empty |
| 4, 5 | `lx`, `ly` (f32) | movement, -1..1, +y = up: the pad's left stick after a 7000/32767 dead zone plus the d-pad (a d-pad direction bound to a camera action does not count); on Keyboard & Mouse also W/S/A/D, as the exe's `read_move_axes` |
| 6, 7 | `rx`, `ry` (f32) | look: the pad's right stick, -1..1, +y = up, zero inside 7000/32767 and not rescaled (the exe's `_deadax`) |
| 8, 9 | `mouseDx`, `mouseDy` (s32) | raw mouse counts since the last game frame (Raw Input, no acceleration), + = right / down; Keyboard & Mouse only, otherwise 0 |
| 10 | `focused` | 1 while the game window has the focus |
| 11 | `uiOpen` | 1 while a REO menu, the pause menu or the camera window takes the input (the exe's `ui_open`); the mod then ignores look and presses nothing |

**Camera actions and their bind settings:**

| Bit | Action | Setting | File 1 default |
|---|---|---|---|
| `0x001` | Aim | `aim_button` | `trig:L` (left trigger) |
| `0x002` | Shoot | `shoot_button` | `trig:R` (right trigger) |
| `0x004` | Reload | `reload_button` | `pad:0x4000` (X / Square) |
| `0x008` | Sprint | `sprint_button` | `pad:0x0040` (L3) |
| `0x010` | Free cursor | `capture_button` | `key:0x43` (C) |
| `0x020` | Recenter | `recenter_button` | none |
| `0x040` | Swap shoulder | `swap_shoulder_button` | `pad:0x0080` (R3) |
| `0x080` | Special action | `special_button` | none |
| `0x100` | Watch next player | `lens_button` | none |
| `0x200` | Alt view | `switch_view_button` | none |
| `0x400` | Reveal camera | `reveal_button` | none |

**Bind codes** (ModernCam's own, `ro_input.button_held`):
- `trig:L` / `trig:R`: the trigger is past 100 of 255.
- `pad:0xNNNN`: an XInput `wButtons` mask:
  - D-pad: Up `0x0001`, Down `0x0002`, Left `0x0004`, Right `0x0008`.
  - Start `0x0010`, Back `0x0020`, L3 `0x0040`, R3 `0x0080`, LB `0x0100`, RB `0x0200`.
  - A `0x1000`, B `0x2000`, X `0x4000`, Y `0x8000`.
  - On a DualShock or DualSense the same positions apply: Cross = A, Circle = B, Square = X, Triangle = Y.
- `key:0xVK`: a Windows virtual key.
- `mb:0xVK`: a mouse button: 0x01 left, 0x02 right, 0x04 middle, 0x05 / 0x06 the side buttons.
- `""`: unbound.

**Rules the exe applies:**
- Key and mouse binds read as held only while `focused`.
- On Keyboard & Mouse, an Aim bind that is a pad or trigger code reads the right mouse button (`bind_for_device`).
- Pad binds read the device chosen by `input_device`.

### 4.6 `reo_cam_input_hide(actions)`
While the mod drives, the physical inputs of these camera actions do not reach the game. The mod passes every held
camera action, which is the exe's raw suppress of its own binds plus `set_raw_suppress`.
- Apply it where REO builds the virtual DualShock from the device, before the pad data reaches the IOP.
- Clear it when the call passes 0; the mod does that whenever it stops driving.

### 4.7 `reo_pad_override(flags)`
These are overrides of the virtual DualShock the game receives, valid until the next call:
- `0x1`: the left stick is at `0x80,0x80`. Used while a reload waits and during aim-walk (the exe's `lx_over` and
  `silence_analog`).
- `0x2`: the right stick is at `0x80,0x80`. This is `own_look_stick`, off on Keyboard & Mouse: the game never sees
  the camera's look stick, so looking around triggers no ad-libs.

The mod also centres the stick groups of her mapped record at the pad-map mark, which is exact inside the game's
frame. The override covers the raw frames the game also reads.

### 4.8 `reo_mouse_capture(on)`
On Keyboard & Mouse, `on = 1` hides the cursor, holds it inside the game window, and makes its motion camera look
(`mouseDx`/`mouseDy`). `on = 0` frees it. The mod toggles this state with the Free cursor action, starting captured
as the exe does.
- Release the capture while the game window has no focus, and while the camera window or a menu is open.
- Pass 0 when the device is not Keyboard & Mouse.

### 4.9 `reo_cam_crosshair`
The mod sends this every driven frame (9 words):

| Word | Field |
|---|---|
| 0 | `visible`: crosshair on and aiming (aim blend > 0.1, the exe's `AIM 1`) |
| 1 | `style`: 0 dot, 1 cross |
| 2 | `size`: 1-16 px (dot radius or arm length) |
| 3 | `gap`: 0-16 px (cross only) |
| 4 | `thickness`: 1-6 px (cross only) |
| 5 | `rgb`: `0x00RRGGBB` |
| 6, 7 | `dx`, `dy`: px (+x right, -y up) |
| 8 | `aimAmount` (f32): 0..1, optional fading |

Draw it over the game picture:
- centred on the displayed game picture (the 4:3 or widescreen rectangle as shown), plus `dx`, `dy`;
- in the output window's pixels, as the exe's overlay window drew it over PCSX2's render area;
- in the colour given.

It is not part of the game's image: frame captures of the game picture do not contain it.

### 4.10 Guest calls at a mark (reload)
The exe reloads through a cave in the game code that calls the item-combine routine inside the game's frame. Here the
mod asks the host to make that call at a mark:

```
typedef struct { u32 function, matchA0, a1, a2, a3, expireVblanks; } ReoGuestCall;
u32 ticket = reo_guest_call_at("camera.reload", &call);
u32 state  = reo_guest_call_result(ticket, &v0);   /* 0 pending, 1 done (v0 written), 2 expired, 3 unknown */
```

- **When.** The call runs at the next execution of the mark named `hook` (a mark site of the feature profile) whose
  `$a0` equals `matchA0`, or any `$a0` when `matchA0` is `0xFFFFFFFF`.
- **How.** The host calls `FeatureHookContext::CallGuest(function, {a0 = the mark's $a0, a1, a2, a3})` before the
  marked instruction runs.
  - The call is native when the build has the function.
  - Every register is restored afterwards.
  - `$v0` is kept for `reo_guest_call_result`.
- **Lifetime.** A call is made once. It expires after `expireVblanks` vblanks without a match. Tickets are unique, and
  results are kept at least 2 s.
- **Refused** (the result is 0 at once):
  - an unknown hook;
  - a function outside the code of an image that is in memory;
  - a mod that is not enabled.
- **What the mod asks.**
  - hook `camera.reload`: game.bin `0x0066B500`, the entry of the per-character update, `a0` = the character record.
    This is the US equivalent of the exe's reload hook at JP `0x006399A0`.
  - function `0x006690E0`: the item-combine routine, JP `0x006375B0`.
  - `matchA0` = the local player's record (`0x004A5C30` offline); `a1` = the weapon's item slot; `a2` = the ammo's
    item slot; `expireVblanks` = 72.
  - `v0 = 0` means the reload started: action `0x1C`, sub-state `0x40`, the game's own animation.
  - Probed on the instruction clock with the exe's own cave (camera_v2 `reload_probe.py`):
    - the empty .45 with a .45 magazine gave 7 rounds;
    - with loose .45 rounds it gave 1 round per call, which matches the exe's `count + 1` log line.
- The mod makes the call at most once per Reload press, and only after she has left the aim stance, as the exe's
  `do_reload` does.

### 4.11 Players
- `reo_local_player_slot()`: the character record the local player drives (0 offline).
- `reo_human_player_slots()`: a bit per record 0-3 that a person drives. Offline, that is only the local one.

"Watch next player" cycles the followed record through these. The rig follows the watched character while control,
aim and heading stay hers. Offline there is nobody else, so the bind does nothing and the log says "needs online play".
`seat_hold_seconds` has no use here, because the host knows the seat.

### 4.12 `reo_cam_status(status, detail)`
This is sent when it changes, for the window's status line (the exe's lamp).

| status | Window text |
|---|---|
| 0 | Off |
| 1 | Waiting for a level |
| 2 | Driving the camera |
| 3 | Handed back to a cutscene (it resumes when she moves) |
| 4 | Game code built without the camera patches: rebuild needed |

| detail bit | Meaning |
|---|---|
| 1 | free gun elevation unavailable |
| 2 | reload pending |
| 4 | Alt view on |
| 8 | watching another player |

The host adds its own states: not installed, disabled in Mods, or the mod stopped.

### 4.13 `reo_log(text)`
One line for REO's log. Only the mod's `[evt]` lines use it, while `debug_log` is on.

## 5. The code sites (`patches.json`)
`patches.json` is a feature-profile fragment for `REOutbreakTools feature-profile build --mod-patches <mod
folder>/patches.json`. `tools/gen_patches.py` writes it from the owner's disc and checks every original word. It holds
instruction words of the owner's disc, so it stays private.

Checked with the features build (`feature-profile build`, the owner's disc, the rate profile): all 54 sites apply and
none conflicts with the rate profile.

| Feature | Sites | Kind |
|---|---|---|
| `camera.freeze` | dispatcher → handler 1 (`0x005BF06C`) | callIf (as the exe) |
| | handler 2 (`0x005BF7B0`): the camera-init call `0x005BF7E8` and its 10 stores to the camera block | callIf + subst nop |
| | handler 3 (`0x005BF300`): the camera-init call `0x005BF348` and its 29 stores | callIf + subst nop |
| | follow handler entry `0x005D9960` (reached only through a callback pointer) | returnIf (as the exe) |
| | follow handler's eye call `0x005D9AC0` | callIf (as the exe) |
| `camera.freeAim` | servo calls `0x00586C08`, `0x00586C20`, `0x00586C38` | callIf (as the exe) |
| | stores `0x00586C6C`, `0x00586C78`, `0x00586C84`, `0x00586C88` | subst nop (as the exe) |
| marks | `camera.gameFrame` exe `0x0019F6C0` (hook `game.frame`), `camera.padMap` exe `0x001B0C5C`, `camera.reload` game.bin `0x0066B500` | mark |

Each site has an `option` member naming the setting that switches it (`enabled`, or `modern_aim, free_aim`) and a
`note`. The parser ignores both.

### Subtitles
The previous camera check reported that the exe's handler-3 freeze hides J's Bar's ad-lib subtitle ("Hmm, doesn't...
look like a customer."). The probes of this update show that the freeze is not the cause.
- **Freeze alone, camera unmoved:** the full exe freeze without moving the camera keeps the subtitle at the same
  vblanks.
- **Camera moved, any freeze:** the subtitle moves or changes, with any freeze. Moving the camera changes which sounds
  the game starts, through its 3D sound: extra random draws at exe `0x001ACC9C` in the sound-start routine
  `0x001ACB60`. The random-gated ad-lib lines then come at other times or are other lines. With only the FOV changed,
  a different line appears ("Maybe I had too much to drink.").
- **Same random state:** with the random state set equal to the unmoved run at vblank 4760, the ad-lib subtitle
  appears at the same vblanks with the camera on, both with the exe's freeze and with v2's narrow one.
- **The real v2 mod:** driving the camera in J's Bar and The Hive (mod in the loop), ad-lib subtitles appear ("Maybe
  I had too much to drink.", "Oh man, oh man, oooh maaan!").

v2 still narrows the freeze of handlers 2 and 3 to their camera writes, as asked. Their rail state stays current, so
the hand-back continues from where the game is, and no non-camera work of a handler is skipped.

## 6. The settings window
- `window_layout.json` (generated with the manifests) lists the exe's tabs in order (Camera, Look, Aiming, Controls),
  then Advanced, which holds every value the exe reads but hides. For each setting it gives the group, whether it is
  one of the exe's 89 values, and a widget hint.
- Widget hints:
  - `bind`: capture the next key, mouse button, pad button or trigger. Store it in the exe's code format (4.5). The
    cross clears it to `""`.
  - `device`: offer "Keyboard & Mouse" (`kbm`), "Gamepad (Auto)" (`auto`), and each connected pad by name (`pad:N`).
  - `color`: a colour picker writing `#rrggbb`.
  - `inherit-switch:K` / `inherit:K`: the Aim and Alt rig values. The value shows Main's until it is moved. Moving it
    sets its `_set` switch on (the exe's per-value inheritance); "inherit" sets the switch off again.
- **Live.** Every change updates the snapshot the mod reads at the next game frame. Nothing is applied by the window
  itself.
- **Reset to defaults** returns every value to ModernCam's File 1 defaults, the defaults of `mod.json`, after keeping
  the previous values as a backup, as the exe does.
- **Credit line:** "Modern Camera: Outbreak ModernCam by Snippy (heysnippy.com), ported for the USA disc".

## 7. What the host must not do
- Write game code for the mod, or let the mod write it.
- Apply the pad transform, overrides or hidden binds while the mod does not drive. The mod clears them itself (0),
  and the host clears them when the mod is disabled or stops.
- Change the game's frame pacing. The camera adds no guest code and no guest instructions. Instruction-clock probes
  keep the game's picture cadence (50 pictures per 100 vblanks in J's Bar) with the camera on. The drawing (VU1 work
  per vblank) is within -11% to +11% of the fixed camera's across the five scenarios, and -4% in J's Bar with the real
  mod. Real-time frame rate must be measured once the runtime runs the mod.
