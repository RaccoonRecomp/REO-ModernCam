#ifndef __MCAM_RIG_H__
#define __MCAM_RIG_H__

#include "mcam_math.h"

// Camera rig math, ported from Outbreak ModernCam's ro_camera helpers. Nothing here touches game
// memory. Angles are degrees; headings are atan2(x, z), so clockwise is negative.

typedef struct {
    f32 x, y, z;
} Mc_Vec3;

// One framing preset (hip, aim or alternate view).
typedef struct {
    f32 distance;   // how far behind her the eye sits
    f32 height;     // lifts the eye and the look-at point together (never tilts)
    f32 shoulder;   // sideways offset, + = over her right shoulder
    f32 lookDrop;   // how far below its own height the camera looks
    f32 pitchTrim;  // constant tilt added to the orbit pitch
    f32 fovDeg;     // vertical field of view
} Mc_Rig;

// ModernCam's RIG_KEYS, in the order of the aim_/alt_ settings.
#define MC_RIG_DISTANCE   0
#define MC_RIG_SHOULDER   1
#define MC_RIG_HEIGHT     2
#define MC_RIG_FOV        3
#define MC_RIG_LOOK_DROP  4
#define MC_RIG_PITCH_TRIM 5
#define MC_RIG_VALUES     6

static inline f32* mc_rig_value(Mc_Rig* rig, u32 i) {
    switch (i) {
        case MC_RIG_DISTANCE: return &rig->distance;
        case MC_RIG_SHOULDER: return &rig->shoulder;
        case MC_RIG_HEIGHT: return &rig->height;
        case MC_RIG_FOV: return &rig->fovDeg;
        case MC_RIG_LOOK_DROP: return &rig->lookDrop;
        default: return &rig->pitchTrim;
    }
}

// Fold an angle into [-180, 180).
static inline f32 mc_angwrap(f32 a) {
    return mc_fmodpf(a + 180.0f, 360.0f) - 180.0f;
}

static inline f32 mc_lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

// Frame-rate-independent blend factor for x += (target - x) * alpha. k is the factor wanted at 60 Hz.
f32 mc_smooth_alpha(f32 k, f32 dt);

// Ease a free-look offset back toward 0 with time constant tau (tau <= 0 holds it).
f32 mc_decay_look(f32 off, f32 dt, f32 tau);

// Ease the free-look offset toward "behind her" and turn the camera with it.
void mc_release_free_look(f32* camYaw, f32* lookOff, f32 dt, f32 tau);

// World-space eye and look-at point for an over-the-shoulder shot around (px, py, pz).
void mc_eye_and_look(f32 px, f32 py, f32 pz, f32 yawDeg, f32 pitchDeg, const Mc_Rig* rig, Mc_Vec3* eye, Mc_Vec3* look);

// Elevation of the direction the camera actually looks, + = up.
f32 mc_view_elevation_deg(f32 pitchDeg, f32 distance, f32 lookDrop);

// Camera-relative heading from the left stick. Returns 1 when the stick is out of the deadzone.
u32 mc_compute_modern_movement(f32 lx, f32 ly, f32 viewYaw, f32 playerYaw, f32 deadzone, f32* desiredHeading, f32* gap);

void mc_blend_rig(const Mc_Rig* hip, const Mc_Rig* aim, f32 t, Mc_Rig* out);

// Decides the heading the follow camera sits behind: the direction she travels, or her facing when
// her velocity cannot be trusted. Keeps a fast copy (backpedal detection) and a slow one (tracked).
typedef struct {
    f32 moveHold;
    f32 fast;
    f32 slow;
    u32 havePos;
    f32 posX, posZ;
    f32 since;
    u32 haveStepYaw;
    f32 stepYaw;
    u32 haveYaw;
    f32 fastYaw;
    f32 moveYaw;
    u32 backpedal;
} Mc_FollowHeading;

void mc_follow_init(Mc_FollowHeading* f);
void mc_follow_reset(Mc_FollowHeading* f);
// Returns whether she is moving; *baseYaw receives the heading to sit behind.
u32 mc_follow_update(Mc_FollowHeading* f, f32 px, f32 pz, f32 playerYaw, f32 dt, u32 inputMoving, u32 aiming, f32* baseYaw);

// Tank-turn direction that closes the gap to a target heading, with hysteresis so she does not
// judder around the target.
typedef struct {
    f32 deadzone;
    f32 settle;
    u32 turning;
} Mc_HeadingTurner;

void mc_turner_init(Mc_HeadingTurner* t);
// gap = angwrap(target - playerYaw). Returns +1 (turn right), -1 (left) or 0.
f32 mc_turner_update(Mc_HeadingTurner* t, f32 gap);

#endif
