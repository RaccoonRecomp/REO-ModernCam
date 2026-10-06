#ifndef __MCAM_CONFIG_H__
#define __MCAM_CONFIG_H__

#include "mcam_rig.h"

// The mod's settings, read from the recomp's mod config every frame (changes apply live, like ModernCam's
// hot-reloaded ro_settings.json). Keys, defaults and ranges are ModernCam's File 1 values (tools/settings.py).
// The binds are resolved by the host (reo_cam_input), so they are not read here.

typedef enum {
    MC_TURN_OFF,
    MC_TURN_MOVING,
    MC_TURN_ALWAYS
} Mc_TurnMode;

// One rig value of the Aim or Alt rig: applies only while its "_set" switch is on (the exe's per-key inheritance).
typedef struct {
    u32 set[MC_RIG_VALUES];
    Mc_Rig rig;
} Mc_RigOverride;

typedef struct {
    u32 enabled;

    // Framing
    Mc_Rig hip;
    Mc_RigOverride aim;
    Mc_RigOverride alt;

    // Following
    u32 keepBehind; // rotate_mode: 1 = follow
    f32 followStrength;
    f32 freeLookReturn;
    f32 followHold;
    u32 followBehind; // follow_dir >= 0
    f32 yawOffset;
    f32 shoulderSwapTime;
    f32 recenterTime;

    // Look
    f32 stickSpeed;
    f32 mouseSensitivity;
    f32 pitchSpeed;
    u32 invertPitch;
    u32 invertStick;
    f32 pitchMin;
    f32 pitchMax;

    // Aiming
    u32 modernAim;
    u32 freeAim;
    u32 aimPitchLock;
    f32 aimSensitivity;
    f32 gunPitchMax;
    f32 aimZeroDeg;
    f32 aimStrafeSpeed;
    f32 aimBlend;
    f32 aimBlendIn;
    f32 aimBlendOut;
    u32 aimFollow;
    f32 aimFollowStrength;
    u32 aimFreeLook;
    u32 aimSnapBehind;
    f32 aimSnapSpeed;
    f32 aimShoulderMin;
    f32 modernAimDeadzone;

    // Crosshair (drawn by the host)
    u32 crosshair;
    u32 crosshairStyle;
    u32 crosshairSize;
    u32 crosshairGap;
    u32 crosshairThickness;
    u32 crosshairRgb;
    s32 crosshairDx;
    s32 crosshairDy;

    // Input behaviour
    u32 sprintToggle; // sprint_mode: 1 = toggle, 0 = hold
    u32 ownLookStick;
    f32 seatHoldSeconds; // the exe's online seat detection; the host knows the local player

    // Turning
    u32 turnToCamera; // Mc_TurnMode
    f32 turnSpeed;
    f32 turnDeadzone;
    u32 ownTurn; // File 2 only in the exe

    // Collision
    u32 camCollision;
    f32 camMargin;
    f32 camBlockTop;
    u32 camIgnoreProps;
    f32 camRecover;
    f32 camHold;
    f32 camSnap;

    // Projection
    f32 nearClip;
    f32 farClip;
    f32 fovScale;

    // World
    u32 cutMatch;
    f32 cutMatchRadius;
    u32 cutMatchMaxCuts;
    u32 showHiddenObjects;
    f32 drawDistance;
    u32 charLight;
    u32 charLightRgb;

    // Doors and cutscenes
    u32 doorHold;
    u32 cinematicYield;
    u32 cinematicYieldTakes;
    f32 cinematicYieldWindow;
    f32 cinematicNeutralS;
    f32 cinematicMoveConfirmS; // dormant in the exe

    // Reload, reveal, log
    u32 reloadAnimated;
    f32 revealTime;
    u32 debugLog;
} Mc_Config;

void mc_config_read(Mc_Config* cfg);

// Hip and aim rigs for this frame (ModernCam's resolve_presets): Main, or Alt while the alt view is on, with each Alt
// value that is set replacing Main's; then Aim values that are set replace that rig's. An aim FOV of 0 uses the
// hip rig's FOV.
void mc_config_presets(const Mc_Config* cfg, u32 altView, Mc_Rig* hip, Mc_Rig* aim);

#endif
