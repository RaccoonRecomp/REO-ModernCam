#include "mcam_config.h"
#include "reo_host.h"

static f32 num(const char* key) {
    return (f32)recomp_get_config_double(key);
}

static u32 opt(const char* key) {
    return recomp_get_config_u32(key);
}

static s32 inum(const char* key) {
    // Integer settings can be negative (crosshair offsets): round the double.
    f64 v = recomp_get_config_double(key);

    return (s32)(v < 0.0 ? v - 0.5 : v + 0.5);
}

static u32 rgb(const char* key, u32 fallback) {
    u32 v = reo_config_get_rgb(key);

    return (v & 0xFF000000u) ? fallback : v;
}

static void read_rig(const char* const keys[MC_RIG_VALUES], Mc_Rig* rig) {
    u32 i;

    for (i = 0; i < MC_RIG_VALUES; i++) {
        *mc_rig_value(rig, i) = num(keys[i]);
    }
}

static void read_override(const char* const keys[MC_RIG_VALUES], const char* const setKeys[MC_RIG_VALUES],
                          Mc_RigOverride* o) {
    u32 i;

    read_rig(keys, &o->rig);
    for (i = 0; i < MC_RIG_VALUES; i++) {
        o->set[i] = opt(setKeys[i]);
    }
}

static const char* const kHipKeys[MC_RIG_VALUES] = { "distance", "shoulder", "height", "fov_deg", "look_drop",
                                                     "pitch_trim" };
static const char* const kAimKeys[MC_RIG_VALUES] = { "aim_distance", "aim_shoulder", "aim_height", "aim_fov_deg",
                                                     "aim_look_drop", "aim_pitch_trim" };
static const char* const kAimSet[MC_RIG_VALUES] = { "aim_distance_set", "aim_shoulder_set", "aim_height_set",
                                                    "aim_fov_deg_set", "aim_look_drop_set", "aim_pitch_trim_set" };
static const char* const kAltKeys[MC_RIG_VALUES] = { "alt_distance", "alt_shoulder", "alt_height", "alt_fov_deg",
                                                     "alt_look_drop", "alt_pitch_trim" };
static const char* const kAltSet[MC_RIG_VALUES] = { "alt_distance_set", "alt_shoulder_set", "alt_height_set",
                                                    "alt_fov_deg_set", "alt_look_drop_set", "alt_pitch_trim_set" };

void mc_config_read(Mc_Config* cfg) {
    cfg->enabled = opt("enabled");

    read_rig(kHipKeys, &cfg->hip);
    read_override(kAimKeys, kAimSet, &cfg->aim);
    read_override(kAltKeys, kAltSet, &cfg->alt);

    cfg->keepBehind = opt("rotate_mode");
    cfg->followStrength = num("follow_strength");
    cfg->freeLookReturn = num("free_look_return");
    cfg->followHold = num("follow_hold");
    cfg->followBehind = opt("follow_dir") == 0; // options: Behind her (1), In front of her (-1)
    cfg->yawOffset = num("yaw_offset");
    cfg->shoulderSwapTime = num("shoulder_swap_time");
    cfg->recenterTime = num("recenter_time");

    cfg->stickSpeed = num("stick_speed");
    cfg->mouseSensitivity = num("mouse_sensitivity");
    cfg->pitchSpeed = num("pitch_speed");
    cfg->invertPitch = opt("invert_pitch");
    cfg->invertStick = opt("invert_stick");
    cfg->pitchMin = num("pitch_min");
    cfg->pitchMax = num("pitch_max");

    cfg->modernAim = opt("modern_aim");
    cfg->freeAim = opt("free_aim");
    cfg->aimPitchLock = opt("aim_pitch_lock");
    cfg->aimSensitivity = num("aim_sensitivity");
    cfg->gunPitchMax = num("gun_pitch_max");
    cfg->aimZeroDeg = num("aim_zero_deg");
    cfg->aimStrafeSpeed = num("aim_strafe_speed");
    cfg->aimBlend = num("aim_blend");
    cfg->aimBlendIn = num("aim_blend_in");
    cfg->aimBlendOut = num("aim_blend_out");
    cfg->aimFollow = opt("aim_follow");
    cfg->aimFollowStrength = num("aim_follow_strength");
    cfg->aimFreeLook = opt("aim_free_look");
    cfg->aimSnapBehind = opt("aim_snap_behind");
    cfg->aimSnapSpeed = num("aim_snap_speed");
    cfg->aimShoulderMin = num("aim_shoulder_min");
    cfg->modernAimDeadzone = num("modern_aim_deadzone");

    cfg->crosshair = opt("crosshair");
    cfg->crosshairStyle = opt("crosshair_style");
    cfg->crosshairSize = opt("crosshair_size");
    cfg->crosshairGap = opt("crosshair_gap");
    cfg->crosshairThickness = opt("crosshair_thickness");
    cfg->crosshairRgb = rgb("crosshair_color", 0xF2F2F2u);
    cfg->crosshairDx = inum("crosshair_dx");
    cfg->crosshairDy = inum("crosshair_dy");

    cfg->sprintToggle = opt("sprint_mode") == 0; // options: Toggle, Hold
    cfg->ownLookStick = opt("own_look_stick");
    cfg->seatHoldSeconds = num("seat_hold_seconds");

    cfg->turnToCamera = opt("turn_to_camera");
    cfg->turnSpeed = num("turn_speed");
    cfg->turnDeadzone = num("turn_deadzone");
    cfg->ownTurn = opt("own_turn");

    cfg->camCollision = opt("cam_collision");
    cfg->camMargin = num("cam_margin");
    cfg->camBlockTop = num("cam_block_top");
    cfg->camIgnoreProps = opt("cam_ignore_props");
    cfg->camRecover = num("cam_recover");
    cfg->camHold = num("cam_hold");
    cfg->camSnap = num("cam_snap");

    cfg->nearClip = num("near_clip");
    cfg->farClip = num("far_clip");
    cfg->fovScale = num("fov_scale");

    cfg->cutMatch = opt("cut_match");
    cfg->cutMatchRadius = num("cut_match_radius");
    cfg->cutMatchMaxCuts = opt("cut_match_max_cuts");
    cfg->showHiddenObjects = opt("show_hidden_objects");
    cfg->drawDistance = num("draw_distance");
    cfg->charLight = opt("char_light");
    cfg->charLightRgb = rgb("char_light_color", 0x808080u);

    cfg->doorHold = opt("door_hold");
    cfg->cinematicYield = opt("cinematic_yield");
    cfg->cinematicYieldTakes = opt("cinematic_yield_takes");
    cfg->cinematicYieldWindow = num("cinematic_yield_window");
    cfg->cinematicNeutralS = (f32)opt("cinematic_neutral_ms") / 1000.0f;
    cfg->cinematicMoveConfirmS = (f32)opt("cinematic_move_confirm_ms") / 1000.0f;

    cfg->reloadAnimated = opt("reload_animated");
    cfg->revealTime = num("reveal_time");
    cfg->debugLog = opt("debug_log");
}

static void apply_override(const Mc_RigOverride* o, Mc_Rig* rig) {
    u32 i;

    for (i = 0; i < MC_RIG_VALUES; i++) {
        if (o->set[i]) {
            *mc_rig_value(rig, i) = *mc_rig_value((Mc_Rig*)&o->rig, i);
        }
    }
}

void mc_config_presets(const Mc_Config* cfg, u32 altView, Mc_Rig* hip, Mc_Rig* aim) {
    *hip = cfg->hip;
    if (altView) {
        apply_override(&cfg->alt, hip);
    }
    *aim = *hip;
    apply_override(&cfg->aim, aim);
    if (!(aim->fovDeg != 0.0f)) {
        aim->fovDeg = hip->fovDeg; // the exe: "if not aim['fov_deg']"
    }
}
