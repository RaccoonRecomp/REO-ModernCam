#ifndef __REO_HOST_H__
#define __REO_HOST_H__

#include "recomp_api.h"

// The REOutbreak host API this mod imports (v2). HOST_API.md is the specification for the host side; this header is
// the mod side of the same contract. Everything is resolved by the game's sandboxed mod runtime ("*" = the base
// game, like recomp_get_config_*).
//
// ABI (O32, big-endian MIPS II as built by the Makefile): every argument is one 32-bit word in $a0-$a3 (a pointer into
// the mod's own memory, a u32/s32, or the IEEE-754 bits of an f32); every result is one 32-bit word in $v0. No host
// function takes or returns a value in an FPU register (except the N64Recomp config call recomp_get_config_double,
// which returns a double in $f0). Structures passed by pointer consist of 32-bit words only and are read and written
// by the host word by word, so the runtime's byte-lane rule never applies to them. Strings are the mod's C strings,
// read by the host the way the runtime reads mod bytes.
//
// Guest memory: EE RAM 0x00000000-0x01FFFFFF is dereferenced directly (mcam_mem.h, aligned words only). The game's
// CODE is never read or written by this mod: code changes are the switchable feature sites of patches.json.

// ---- Events the mod exports (the runtime calls them) ------------------------------------------------------------
// recomp_on_play_main(void)                       once per game frame, at the game's frame mark (exe 0x0019F6C0).
// recomp_on_pad_map(u32 heldWordAddr, u32 charRecord)
//                                                 at the camera.padMap mark (exe 0x001B0C5C, before the mapper's
//                                                 lw t4,0(a2)): heldWordAddr = $a2 (the mapped record's +0x08 word
//                                                 the mapper is about to load), charRecord = $s0 (the character
//                                                 record being mapped). Once per character per game frame.

// ---- Feature switches (the code sites of patches.json) -----------------------------------------------------------
#define REO_FEATURE_CAMERA_FREEZE "camera.freeze"
#define REO_FEATURE_CAMERA_FREEAIM "camera.freeAim"
#define REO_FEATURE_OFF         0 // switched off
#define REO_FEATURE_ACTIVE      1 // on, and the code of all its sites is in memory
#define REO_FEATURE_WAITING     2 // on, but an overlay its sites patch is not loaded now
#define REO_FEATURE_UNAVAILABLE 3 // cannot be switched on (the game code was built without its sites, a word differs)
#define REO_FEATURE_UNKNOWN     4 // no feature of that name
// Ask for a feature on (1) or off (0). Takes effect at the next vblank start. Returns the state after the request
// (REO_FEATURE_*) as the host knows it at the call.
RECOMP_IMPORT("*", u32 reo_feature_set(const char* name, u32 on));
RECOMP_IMPORT("*", u32 reo_feature_state(const char* name));
// 1 while the code of the named image of the feature profile ("game.bin") is in memory.
RECOMP_IMPORT("*", u32 reo_image_loaded(const char* image));

// ---- Time --------------------------------------------------------------------------------------------------------
// The field rate in millihertz: 59940 (NTSC) or 60000 (the owner's FieldRate = 60hz).
RECOMP_IMPORT("*", u32 reo_vblank_rate_mhz(void));

// ---- Settings ----------------------------------------------------------------------------------------------------
// Parse a colour setting ("#rgb", "#rrggbb", '#' optional, as ModernCam's _hex_rgb): 0x00RRGGBB, or 0xFFFFFFFF
// when the value is not a colour (the mod then uses the default, as the exe).
RECOMP_IMPORT("*", u32 reo_config_get_rgb(const char* key));
// Store a number setting (the IEEE-754 bits of an f32): the value the window and mod_config.json show. ModernCam's
// swap shoulder writes the new side back into its settings this way.
RECOMP_IMPORT("*", void reo_config_set_float(const char* key, u32 floatBits));

// ---- Camera input (the physical device chosen by input_device; binds resolved by the host) ----------------------
#define REO_CAM_INPUT_VERSION 1
#define REO_CAM_DEVICE_NONE 0
#define REO_CAM_DEVICE_PAD  1
#define REO_CAM_DEVICE_KBM  2
// Camera actions (bit per bind setting). The host resolves each *_button setting (ModernCam's codes: "trig:L",
// "trig:R", "pad:0x0040", "key:0x43", "mb:0x02", "" = unbound) against the chosen device.
#define REO_CAM_ACT_AIM     0x0001 // aim_button (on Keyboard & Mouse a pad/trigger bind is the right mouse button)
#define REO_CAM_ACT_SHOOT   0x0002 // shoot_button
#define REO_CAM_ACT_RELOAD  0x0004 // reload_button
#define REO_CAM_ACT_SPRINT  0x0008 // sprint_button
#define REO_CAM_ACT_CAPTURE 0x0010 // capture_button ("Free cursor")
#define REO_CAM_ACT_RECENTER 0x0020 // recenter_button
#define REO_CAM_ACT_SWAP    0x0040 // swap_shoulder_button
#define REO_CAM_ACT_SPECIAL 0x0080 // special_button
#define REO_CAM_ACT_LENS    0x0100 // lens_button ("Watch next player")
#define REO_CAM_ACT_ALTVIEW 0x0200 // switch_view_button ("Alt view")
#define REO_CAM_ACT_REVEAL  0x0400 // reveal_button
#define REO_CAM_ACT_ALL     0x07FF

typedef struct {
    u32 version;      // REO_CAM_INPUT_VERSION
    u32 device;       // REO_CAM_DEVICE_*
    u32 held;         // REO_CAM_ACT_* held now
    u32 bound;        // REO_CAM_ACT_* whose setting is not empty
    f32 lx, ly;       // movement: left stick + d-pad (+ the keys REO maps to them), -1..1, +y = up, after the dead zone
    f32 rx, ry;       // look: right stick, -1..1, +y = up, after ModernCam's 7000/32767 dead zone (no rescale)
    s32 mouseDx;      // raw mouse counts since the last game frame (Keyboard & Mouse only, 0 otherwise), + = right
    s32 mouseDy;      //                                                                                 + = down
    u32 focused;      // 1 while the game window has the focus (key and mouse binds read only then, as the exe)
    u32 uiOpen;       // 1 while a REO menu or the camera window takes the input (the exe's ui_open): no look
} ReoCamInput;
// Fill *out for this game frame. Returns 1 when valid, 0 when the host has no camera input service.
RECOMP_IMPORT("*", u32 reo_cam_input(ReoCamInput* out));
// The physical inputs of these camera actions do not reach the game until the next call (ModernCam's raw suppress
// of its own binds). Called once per game frame.
RECOMP_IMPORT("*", void reo_cam_input_hide(u32 actions));
// Overrides of the virtual DualShock the game receives, until the next call (ModernCam's pad-shadow overrides).
#define REO_PAD_CENTRE_LEFT  0x1 // left stick at 0x80,0x80
#define REO_PAD_CENTRE_RIGHT 0x2 // right stick at 0x80,0x80 (0x7F7F in ModernCam: "own look stick", no ad-libs)
RECOMP_IMPORT("*", void reo_pad_override(u32 flags));
// Keyboard & Mouse: 1 = the cursor is hidden and held in the game window and its motion is camera look; 0 = free.
RECOMP_IMPORT("*", void reo_mouse_capture(u32 on));

// ---- Crosshair (drawn by the host over the game picture) ---------------------------------------------------------
#define REO_XHAIR_DOT   0
#define REO_XHAIR_CROSS 1
typedef struct {
    u32 visible;      // crosshair on and aiming (ModernCam's AIM 1: aim blend > 0.1) while the camera drives
    u32 style;        // REO_XHAIR_*
    u32 size;         // px: dot radius / arm length, 1-16
    u32 gap;          // px, cross only, 0-16
    u32 thickness;    // px, cross only, 1-6
    u32 rgb;          // 0x00RRGGBB
    s32 dx, dy;       // px offset from the centre of the displayed game picture (+x right, -y up)
    f32 aimAmount;    // 0..1, the aim blend (for fading, optional)
} ReoCrosshair;
RECOMP_IMPORT("*", void reo_cam_crosshair(const ReoCrosshair* crosshair));

// ---- Guest calls at a mark (ModernCam's reload cave) -------------------------------------------------------------
#define REO_GUEST_ANY_A0 0xFFFFFFFFu
typedef struct {
    u32 function;     // guest function address (inside the loaded game code)
    u32 matchA0;      // run at the first execution of the mark whose $a0 equals this (REO_GUEST_ANY_A0: any)
    u32 a1, a2, a3;   // the call's $a1-$a3; its $a0 is the mark's $a0
    u32 expireVblanks;// give up when the mark has not matched within this many vblanks
} ReoGuestCall;
#define REO_CALL_PENDING 0
#define REO_CALL_DONE    1
#define REO_CALL_EXPIRED 2
#define REO_CALL_UNKNOWN 3
// Queue one call; returns a ticket (non-zero) or 0 when refused (unknown hook, function outside the game code).
RECOMP_IMPORT("*", u32 reo_guest_call_at(const char* hook, const ReoGuestCall* call));
// State of a ticket (REO_CALL_*); when done, *v0 receives the function's $v0.
RECOMP_IMPORT("*", u32 reo_guest_call_result(u32 ticket, u32* v0));

// ---- Players -----------------------------------------------------------------------------------------------------
// The character record the local player drives (0 offline) and the records driven by people (bit per record 0-3;
// offline only the local one). ModernCam's seat detection and "Watch next player" use them.
RECOMP_IMPORT("*", u32 reo_local_player_slot(void));
RECOMP_IMPORT("*", u32 reo_human_player_slots(void));

// ---- Status and log ----------------------------------------------------------------------------------------------
#define REO_CAM_STATUS_OFF         0 // switched off
#define REO_CAM_STATUS_WAITING     1 // waiting for a level
#define REO_CAM_STATUS_DRIVING     2 // driving the camera
#define REO_CAM_STATUS_YIELDED     3 // handed back to a cutscene until she moves
#define REO_CAM_STATUS_UNAVAILABLE 4 // the game code was built without the camera's feature sites
// detail: REO_CAM_DETAIL_* bits for the window's status line.
#define REO_CAM_DETAIL_FREEAIM_UNAVAILABLE 0x1
#define REO_CAM_DETAIL_RELOAD_PENDING      0x2
#define REO_CAM_DETAIL_ALT_VIEW            0x4
#define REO_CAM_DETAIL_WATCHING_OTHER      0x8
RECOMP_IMPORT("*", void reo_cam_status(u32 status, u32 detail));
// One event line for REO's log (debug_log; ModernCam's "[evt]" lines).
RECOMP_IMPORT("*", void reo_log(const char* text));

#endif
