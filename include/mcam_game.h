#ifndef __MCAM_GAME_H__
#define __MCAM_GAME_H__

#include "mcam_rig.h"

// Game access for Resident Evil Outbreak (NTSC-U) v2.00, ported from ModernCam's ro_addresses (File 1).
// v2: data only. The code ModernCam patched is switched through the host's feature switches (patches.json).

// ---- Feature switches (the code sites of patches.json) ------------------------------------------------------
// Request a feature on or off; cheap when nothing changes (the request is sent when it differs from the last one).
// Returns the host's state (REO_FEATURE_*).
u32 mc_feature(u32 which, u32 on);
u32 mc_feature_state(u32 which);
#define MC_FEAT_FREEZE  0
#define MC_FEAT_FREEAIM 1
#define MC_FEAT_COUNT   2
// Forget what was requested (the host was restarted / a new session): the next mc_feature() call is sent.
void mc_feature_forget(void);
// game.bin (the overlay whose camera code the freeze switches) is in memory.
u32 mc_game_loaded(void);

// ---- Characters ------------------------------------------------------------------------------------------------
// The local player's record (0 offline, from the host) and the record the camera follows (the local one, or another
// human player's while "Watch next player" is on).
void mc_set_player_slot(u32 slot);
u32 mc_player_slot(void);
void mc_set_view_slot(u32 slot);
u32 mc_view_slot(void);
u32 mc_char_base(u32 slot);
u32 mc_char_live(u32 slot); // position read back sane and not the origin
u32 mc_level_ready(void);   // the local player's record is live
void mc_view_pos(f32* x, f32* y, f32* z);
f32 mc_view_yaw_deg(void);
void mc_player_pos(f32* x, f32* y, f32* z);
void mc_set_player_pos(f32 x, f32 y, f32 z);
f32 mc_player_yaw_deg(void);        // from the model matrix: the way she faces
f32 mc_player_heading_deg(void);    // from the heading field
void mc_set_player_heading(f32 deg);
void mc_set_aim_angle_deg(f32 deg);
u32 mc_in_aim_state(void);
u32 mc_busy_with_action(void);
f32 mc_aim_move_step(f32 right, f32 forward, f32 dt, f32 speed);

// ---- Camera ----------------------------------------------------------------------------------------------------
void mc_write_camera(const Mc_Vec3* eye, const Mc_Vec3* look);
void mc_read_camera_eye(Mc_Vec3* eye);
f32 mc_read_fov_deg(void);
void mc_write_fov_deg(f32 deg);
void mc_write_near_far(f32 nearClip, f32 farClip); // <= 0 leaves that value alone
f32 mc_read_far(void);
void mc_write_aspect(f32 scale);
void mc_restore_aspect(void);

// ---- Cuts (the room's authored fixed cameras) -------------------------------------------------------------------
u32 mc_cut_count(void);
u32 mc_cut_index(void);
u32 mc_cut_eye(u32 index, Mc_Vec3* eye);
// The current cut's authored eye, target and FOV (ModernCam's cut_pose; the reveal camera and the hand-back).
u32 mc_cut_pose(Mc_Vec3* eye, Mc_Vec3* look, f32* fov);
void mc_force_cut(u32 index);
void mc_release_cut(void);

// ---- Visibility and lighting ------------------------------------------------------------------------------------
u32 mc_show_all_objects(void);
// The near-camera hide radii (reo_camera_us.h): scaled from the originals, never above them.
void mc_set_near_hide_scale(f32 scale);
void mc_restore_near_hide(void);
void mc_set_entity_light(u32 rgb);
void mc_restore_entity_light(void);

// ---- Pad ---------------------------------------------------------------------------------------------------------
typedef struct {
    u32 valid;
    u32 held;     // SCE bits, active-high
    f32 lx, ly;   // [-1, 1], +y = up
    f32 rx, ry;
} Mc_Pad;

// Pad 1's newest raw DualShock frame (the fallback when the host gives no camera input).
void mc_pad_read(Mc_Pad* pad);
// At the pad-map mark: `held = (held & ~suppress) | press` on the word the mapper is about to load (SCE bits, only
// the seven with known source bits act; press beats suppress), and the stick groups centred, before the mapper
// copies them to her action block (ModernCam's VBTN cave).
void mc_padmap_apply(u32 heldWordAddr, u32 pressSce, u32 suppressSce, u32 centreLeft, u32 centreRight);
// The mapped record's held word of the local player (the address the mapper's $a2 holds for her).
u32 mc_padmap_player_word(void);
// SCE bit of the button that performs an action under the current control type (0 if none).
u32 mc_button_for_action(u32 actionBit);

// ---- Items (reload) -------------------------------------------------------------------------------------------------
typedef struct {
    u32 weaponSlot;
    u32 ammoSlot;
    u32 count;   // rounds in the weapon now
    u32 cap;     // its capacity
    u32 candidates;
} Mc_ReloadPlan;
// ModernCam's find_reload for the local player: the equipped weapon with room in it, and the carried item to load,
// loose rounds first. Returns 1 with a plan, 0 when there is nothing to do.
u32 mc_find_reload(Mc_ReloadPlan* plan);

#endif
