#include "mcam_config.h"
#include "mcam_game.h"
#include "mcam_collision.h"
#include "mcam_mem.h"
#include "reo_camera_us.h"
#include "reo_host.h"

// The camera driver: a port of Outbreak ModernCam's ro_camera.run() for File 1, called once per game frame (the
// frame mark) instead of polling PCSX2 over PINE. Everything ModernCam needed only because it ran outside the
// emulator (PINE reconnects, sentinel code writes, persisted original instructions, the 200 Hz pad-shadow thread, the
// crosshair overlay window) is replaced by the host services of reo_host.h; the camera logic, its constants and its
// order of operations are ModernCam's.
//
// v2 against v1: no code writes (feature switches of patches.json); ModernCam's device input, binds, mouse look,
// free cursor, crosshair, reload, special action, reveal camera, watch next player, per-value Aim/Alt rigs and the
// timings v1 hard-coded; the pad transform applied at the camera.padMap mark (ModernCam's VBTN cave); draw distance
// capped at 1, so it never hides nearby characters.

#define MC_MAX_DT        0.1f
#define MC_REFREEZE_DIST 25.0f  // the engine moved the camera if the eye is this far from ours
#define MC_HEADING_EPS   0.4f   // degrees; smaller heading corrections are not written
#define MC_LOOK_THRESH   0.12f
#define MC_MOVE_DEADZONE 0.2f
#define MC_MAX_CUTS      64
#define MC_MAX_TAKES     16
#define MC_RELOAD_WAIT   1.2f   // the exe gives up waiting for the aim stance to end after 1.2 s
#define MC_RELOAD_GRACE  0.6f
#define MC_RELOAD_EXPIRE 72u    // vblanks the host keeps a reload call queued (the exe polls 12 x 50 ms)
#define MC_PADMAP_MISSING 30u   // game frames without a pad-map call before the per-frame fallback

typedef struct {
    u32 press;   // SCE bits the game gets pressed
    u32 supp;    // SCE bits taken from the game
    u32 centreL;
    u32 centreR;
    u32 active;
} Mc_PadMap;

typedef struct {
    u32 init;
    u32 prevFrames;
    f32 now;
    f32 vblankHz;

    u32 levelLive;
    u32 frozen;
    u32 unavailable;
    u32 status;
    u32 statusDetail;


    // Engine state the mod owns, so it can be handed back.
    u32 freeAimOn;
    f32 nearScale;
    u32 lightOn;
    u32 aspectOn;
    u32 haveClipOrig;
    f32 nearOrig, farOrig;

    // Rig
    f32 camYaw;
    f32 pitch;
    f32 lookOff;
    f32 camT;
    f32 camHoldTimer;
    f32 aimAmt;
    u32 aimPrev;
    u32 aimWas;
    f32 recenterTimer;
    u32 haveShoulder;
    f32 shoulderTarget;
    f32 shoulderSign;
    u32 altView;
    f32 revealT;
    s32 revealDir;
    Mc_FollowHeading follow;

    // Input
    u32 prevActs;
    u32 captureOn;
    u32 sprintOn;
    u32 prevSprint;
    u32 wasMoving;
    u32 prevAimBind;
    u32 prevMove;
    Mc_PadMap map;
    u32 padMapCalls;
    u32 framesSincePadMap;
    u32 padMapFallbackLogged;

    // Reload (the exe's reload_drops_aim / reload_pending / reload_grace)
    u32 reloadDropsAim;
    u32 reloadPending;
    f32 reloadPendingT;
    f32 reloadGrace;
    u32 reloadTicket;

    // Steering
    u32 haveSteerYaw;
    f32 steerYaw;

    // Doors and cutscenes
    u32 haveLastWritten;
    Mc_Vec3 lastWritten;
    u32 divCount;
    f32 cineTakes[MC_MAX_TAKES];
    u32 cineTakeCount;
    u32 cineYield;
    u32 haveCineNeutral;
    f32 cineNeutralSince;
    u32 haveCineRef;
    f32 cineRefX, cineRefZ;

    // Cut matching
    s32 cutForced;
    s32 cutCand;
    f32 cutCandT;
    u32 haveCutEyes;
    u32 cutEyesN;
    f32 cutEyesT;
    Mc_Vec3 cutEyes[MC_MAX_CUTS];

    u32 debugLog;
} Mc_State;

static Mc_State s;

// ---- small helpers ---------------------------------------------------------------------------------------------

static f32 dist_xz(const Mc_Vec3* a, const Mc_Vec3* b) {
    return mc_hypotf(a->x - b->x, a->z - b->z);
}

static f32 dist3(const Mc_Vec3* a, const Mc_Vec3* b) {
    f32 dx = a->x - b->x;
    f32 dy = a->y - b->y;
    f32 dz = a->z - b->z;

    return mc_sqrtf(dx * dx + dy * dy + dz * dz);
}

static u32 f2bits(f32 v) {
    Mc_Word w;

    w.f = v;
    return w.u;
}

// The exe's debug_log "[evt]" lines, into REO's log. A number is appended in decimal when withNum is set.
static void evt(const char* text, u32 withNum, s32 num) {
    char buf[96];
    u32 n = 0;
    u32 i;

    if (!s.debugLog) {
        return;
    }
    for (i = 0; text[i] && n < 80; i++) {
        buf[n++] = text[i];
    }
    if (withNum) {
        char digits[12];
        u32 d = 0;
        u32 v = num < 0 ? (u32)(-num) : (u32)num;

        buf[n++] = ' ';
        if (num < 0) {
            buf[n++] = '-';
        }
        do {
            digits[d++] = (char)('0' + v % 10);
            v /= 10;
        } while (v && d < 11);
        while (d) {
            buf[n++] = digits[--d];
        }
    }
    buf[n] = 0;
    reo_log(buf);
}

static void send_status(void) {
    static u32 lastStatus = 0xFFFFFFFFu;
    static u32 lastDetail = 0xFFFFFFFFu;

    if (s.status != lastStatus || s.statusDetail != lastDetail) {
        reo_cam_status(s.status, s.statusDetail);
        lastStatus = s.status;
        lastDetail = s.statusDetail;
    }

}

static void input_neutral(void) {
    // Nothing of the camera reaches the game's input.
    s.map.active = 0;
    s.map.press = s.map.supp = s.map.centreL = s.map.centreR = 0;
    s.framesSincePadMap = 0; // the mapper does not run for her outside a level: not a missing pad-map event
    reo_cam_input_hide(0);
    reo_pad_override(0);
    reo_mouse_capture(0);
}

static void crosshair_off(void) {
    ReoCrosshair xh;

    xh.visible = 0;
    xh.style = REO_XHAIR_DOT;
    xh.size = 0;
    xh.gap = 0;
    xh.thickness = 0;
    xh.rgb = 0;
    xh.dx = 0;
    xh.dy = 0;
    xh.aimAmount = 0.0f;
    reo_cam_crosshair(&xh);
}

static void reset_rig_to_player(void) {
    mc_follow_reset(&s.follow);
    s.camYaw = mc_view_yaw_deg() + 180.0f; // the exe seeds behind her whatever follow_dir says
    s.lookOff = 0.0f;
    s.pitch = 0.0f;
    s.haveSteerYaw = 0;
    s.haveLastWritten = 0;
}

// Put back the room's own camera for the current cut: the pose, FOV and clip values the fixed-camera handler would
// have written, so the game does not keep our view until the next cut change.
static void restore_engine_camera(void) {
    Mc_Vec3 eye, look;
    f32 fov;

    if (mc_cut_pose(&eye, &look, &fov)) {
        mc_write_camera(&eye, &look);
        if (Mc_IsSaneF32(fov) && fov > 0.0f) {
            mc_write_fov_deg(fov);
        }
    }
    if (s.haveClipOrig) {
        Mc_WriteF32(REO_US_CAM_NEAR, s.nearOrig);
        Mc_WriteF32(REO_US_CAM_FAR, s.farOrig);
        s.haveClipOrig = 0;
    }
}

// Hand the camera back to the engine (the freeze only). restoreCam: also put the current cut's camera back; not
// wanted while a room is loading, when the next room's cut replaces it anyway.
static void unfreeze(u32 restoreCam) {
    if (s.frozen) {
        mc_feature(MC_FEAT_FREEZE, 0);
        s.frozen = 0;
        if (restoreCam) {
            restore_engine_camera();
        }
    }
}

static void release_reload(void) {
    s.reloadDropsAim = 0;
    s.reloadPending = 0;
    s.reloadGrace = 0.0f;
    s.reloadTicket = 0;
}

// Undo everything the mod changed (the exe's shutdown order).
static void hand_back_all(void) {
    input_neutral();
    crosshair_off();
    if (s.cutForced >= 0) {
        mc_release_cut();
    }
    s.cutForced = s.cutCand = -1;
    if (s.freeAimOn) {
        mc_set_aim_angle_deg(0.0f);
        s.freeAimOn = 0;
    }
    mc_feature(MC_FEAT_FREEAIM, 0);
    mc_restore_near_hide();
    s.nearScale = 1.0f;
    if (s.lightOn) {
        mc_restore_entity_light();
        s.lightOn = 0;
    }
    if (s.aspectOn) {
        mc_restore_aspect();
        s.aspectOn = 0;
    }
    release_reload();
    unfreeze(s.levelLive);
    mc_feature(MC_FEAT_FREEZE, 0);
    s.levelLive = 0;
    s.cineYield = 0;
}

static void freeze(void) {
    if (!s.haveClipOrig) {
        s.nearOrig = Mc_ReadF32(REO_US_CAM_NEAR);
        s.farOrig = Mc_ReadF32(REO_US_CAM_FAR);
        s.haveClipOrig = 1;
    }
    mc_feature(MC_FEAT_FREEZE, 1);
    s.frozen = 1;
}

static void sync_engine_state(const Mc_Config* cfg) {
    f32 wantNear = mc_clampf(cfg->drawDistance, 0.1f, 1.0f);
    u32 wantFreeAim = cfg->modernAim && cfg->freeAim;

    // Draw distance (ModernCam's set_cull_scale): the table it scales is the near-camera hide radii on this disc.
    // Below 1 they shrink as in the exe; they are never enlarged (that hid nearby characters), so a saved value above
    // 1 acts as 1. Nothing else scales: the game's scenes, sky included, lie within its far plane (50,000; probed
    // x4 and x50 in J's Bar, Hellfire's Apple Inn square and Decisions, Decisions' exterior shots).
    if (mc_absf(wantNear - s.nearScale) > 1e-6f) {
        if (wantNear >= 1.0f) {
            mc_restore_near_hide();
        } else {
            mc_set_near_hide_scale(wantNear);
        }
        s.nearScale = wantNear;
        evt("[evt] near-hide radii x1000:", 1, (s32)(wantNear * 1000.0f));
    }

    // The engine rewrites the flags on every cut change, so holding them means re-applying.
    if (cfg->showHiddenObjects) {
        mc_show_all_objects();
    }

    if (wantFreeAim != s.freeAimOn) {
        if (!wantFreeAim) {
            mc_set_aim_angle_deg(0.0f);
        }
        s.freeAimOn = wantFreeAim;
    }
    // The free-aim sites (patches.json camera.freeAim): the host applies them whenever game.bin is in memory.
    if (mc_feature(MC_FEAT_FREEAIM, s.freeAimOn) == REO_FEATURE_UNAVAILABLE) {
        s.statusDetail |= REO_CAM_DETAIL_FREEAIM_UNAVAILABLE;
    }

    if (cfg->charLight) {
        mc_set_entity_light(cfg->charLightRgb);
        s.lightOn = 1;
    } else if (s.lightOn) {
        mc_restore_entity_light();
        s.lightOn = 0;
    }
}

// The exe's near_clip / far_clip, written when > 0 (0 keeps the game's values).
static void write_clip_planes(const Mc_Config* cfg) {
    mc_write_near_far(cfg->nearClip, cfg->farClip);
}

// The engine took the camera (doors, scripted shots). Returns 1 when the camera was handed back
// for a cinematic and this frame must not write it.
static u32 check_camera_taken(const Mc_Config* cfg) {
    Mc_Vec3 cur;
    u32 i, n;

    if (!cfg->doorHold || !s.haveLastWritten) {
        return 0;
    }
    mc_read_camera_eye(&cur);
    if (dist3(&cur, &s.lastWritten) <= MC_REFREEZE_DIST) {
        s.divCount = 0;
        return 0;
    }
    if (++s.divCount < 2) {
        return 0;
    }
    if (cfg->cinematicYield && !s.cineYield) {
        f32 window = mc_maxf(0.2f, cfg->cinematicYieldWindow);
        u32 need = cfg->cinematicYieldTakes < 2 ? 2 : cfg->cinematicYieldTakes;

        // Keep the takes inside the window, then add this one.
        for (i = 0, n = 0; i < s.cineTakeCount; i++) {
            if (s.now - s.cineTakes[i] < window) {
                s.cineTakes[n++] = s.cineTakes[i];
            }
        }
        s.cineTakeCount = n;
        if (s.cineTakeCount < MC_MAX_TAKES) {
            s.cineTakes[s.cineTakeCount++] = s.now;
        }
        if (s.cineTakeCount >= need) {
            // Hand everything back: freeze, cut, draw distance, hidden objects, input transforms.
            s.cineYield = 1;
            s.cineTakeCount = 0;
            s.haveCineNeutral = 0;
            s.haveCineRef = 0;
            s.divCount = 0;
            mc_feature(MC_FEAT_FREEZE, 0);
            s.frozen = 0;
            if (s.cutForced >= 0) {
                mc_release_cut();
            }
            mc_restore_near_hide();
            s.nearScale = 1.0f;
            s.cutForced = s.cutCand = -1;
            s.haveLastWritten = 0;
            s.haveSteerYaw = 0;
            input_neutral();
            crosshair_off();
            release_reload();
            evt("[evt] cinematic: camera handed back", 0, 0);
            return 1;
        }
    }
    // Take it back.
    freeze();
    if (cfg->showHiddenObjects) {
        mc_show_all_objects();
    }
    s.divCount = 0;
    evt("[evt] camera taken by the game: taken back", 0, 0);
    return 0;
}

// While yielded to a cinematic: resume once the stick has been neutral for a moment and then moves
// her by more than a unit. Returns 1 while still yielded.
static u32 cinematic_wait(const Mc_Config* cfg, f32 lx, f32 ly) {
    f32 cx, cy, cz;
    u32 pushing = mc_hypotf(lx, ly) > 0.2f;
    f32 neutralS = mc_maxf(0.05f, cfg->cinematicNeutralS);

    mc_player_pos(&cx, &cy, &cz);
    if (!s.haveCineNeutral) {
        if (!pushing) {
            s.haveCineNeutral = 1;
            s.cineNeutralSince = s.now;
        }
    } else if (s.now - s.cineNeutralSince < neutralS) {
        if (pushing) {
            s.haveCineNeutral = 0;
        }
    } else if (!pushing) {
        s.haveCineRef = 0;
    } else if (!s.haveCineRef) {
        s.haveCineRef = 1;
        s.cineRefX = cx;
        s.cineRefZ = cz;
    } else if (mc_hypotf(cx - s.cineRefX, cz - s.cineRefZ) > 1.0f) {
        s.cineYield = 0;
        s.haveCineNeutral = 0;
        s.haveCineRef = 0;
        s.divCount = 0;
        s.haveLastWritten = 0;
        mc_follow_reset(&s.follow);
        freeze();
        evt("[evt] cinematic over: camera resumed", 0, 0);
    }
    return s.cineYield;
}

static void cut_match(const Mc_Config* cfg, const Mc_Vec3* eye) {
    u32 n, i;
    s32 want = -1;
    f32 bestD = 0.0f;

    if (!cfg->cutMatch) {
        if (s.cutForced >= 0) {
            mc_release_cut();
        }
        s.cutForced = s.cutCand = -1;
        return;
    }

    n = mc_cut_count();
    if (!s.haveCutEyes || n != s.cutEyesN || s.now - s.cutEyesT > 2.0f) {
        if (n != s.cutEyesN) {
            s.cutForced = s.cutCand = -1;
        }
        for (i = 0; i < n && i < MC_MAX_CUTS; i++) {
            mc_cut_eye(i, &s.cutEyes[i]);
        }
        s.haveCutEyes = 1;
        s.cutEyesN = n;
        s.cutEyesT = s.now;
    }
    if (n > MC_MAX_CUTS) {
        n = MC_MAX_CUTS;
    }

    if (n > cfg->cutMatchMaxCuts) {
        // Too many cuts to match by position.
        if (s.cutForced >= 0) {
            mc_release_cut();
            s.cutForced = s.cutCand = -1;
        }
        return;
    }

    for (i = 0; i < n; i++) {
        f32 d = dist_xz(&s.cutEyes[i], eye);

        if (want < 0 || d < bestD) {
            want = (s32)i;
            bestD = d;
        }
    }

    if (want < 0) {
        if (s.cutForced >= 0) {
            mc_release_cut();
            s.cutForced = s.cutCand = -1;
        }
    } else if (s.cutForced < 0) {
        u32 engineCut = mc_cut_index();
        f32 dEngine = engineCut < n ? dist_xz(&s.cutEyes[engineCut], eye) : 1e30f;

        if (want != s.cutCand) {
            s.cutCand = want;
            s.cutCandT = s.now;
        }
        if (bestD <= cfg->cutMatchRadius && bestD < dEngine * 0.5f && s.now - s.cutCandT >= 0.35f &&
            (u32)want != engineCut) {
            mc_force_cut((u32)want);
            s.cutForced = want;
            evt("[evt] cut match ->", 1, want);
        }
    } else if (dist_xz(&s.cutEyes[s.cutForced], eye) > cfg->cutMatchRadius * 1.5f) {
        mc_release_cut();
        s.cutForced = s.cutCand = -1;
    } else {
        f32 dForced = dist_xz(&s.cutEyes[s.cutForced], eye);

        if (want != s.cutCand) {
            s.cutCand = want;
            s.cutCandT = s.now;
        }
        if (want != s.cutForced && bestD <= cfg->cutMatchRadius && bestD < dForced * 0.85f &&
            s.now - s.cutCandT >= 0.35f) {
            mc_force_cut((u32)want);
            s.cutForced = want;
            evt("[evt] cut match ->", 1, want);
        }
    }
}

// "Watch next player": the next human player's record after the one watched now (the exe's cycle_lens over its
// live slots). The host names the human players; offline that is only the local one, so nothing changes.
static void cycle_lens(void) {
    u32 humans = reo_human_player_slots() | (1u << mc_player_slot());
    u32 cur = mc_view_slot();
    u32 k;

    for (k = 1; k <= 4; k++) {
        u32 slot = (cur + k) & 3;

        if ((humans & (1u << slot)) && mc_char_live(slot)) {
            if (slot != cur) {
                mc_set_view_slot(slot);
                mc_follow_reset(&s.follow);
                evt("[evt] watching player", 1, (s32)slot + 1);
            } else {
                evt("[evt] watch next player: no other player to watch (needs online play)", 0, 0);
            }
            return;
        }
    }
}

// Reload (the exe's do_reload): ask the host to call the game's item-combine routine for her at the camera.reload
// mark, where ModernCam's cave called it. Returns 1 when the call was queued.
static u32 do_reload(const Mc_Config* cfg) {
    Mc_ReloadPlan plan;
    ReoGuestCall call;

    if (!cfg->reloadAnimated) {
        evt("[evt] reload: off (Animated Reload)", 0, 0);
        return 0;
    }
    if (!mc_find_reload(&plan)) {
        evt("[evt] reload: nothing to do", 0, 0);
        return 0;
    }
    call.function = REO_US_ITEM_COMBINE_FN;
    call.matchA0 = mc_char_base(mc_player_slot());
    call.a1 = plan.weaponSlot;
    call.a2 = plan.ammoSlot;
    call.a3 = 0;
    call.expireVblanks = MC_RELOAD_EXPIRE;
    s.reloadTicket = reo_guest_call_at(REO_HOOK_RELOAD, &call);
    if (!s.reloadTicket) {
        evt("[evt] reload: the host refused the call", 0, 0);
        return 0;
    }
    evt("[evt] reloading from item slot", 1, (s32)plan.ammoSlot);
    return 1;
}

static void poll_reload(void) {
    u32 v0 = 0xFFFFFFFFu;
    u32 st;

    if (!s.reloadTicket) {
        return;
    }
    st = reo_guest_call_result(s.reloadTicket, &v0);
    if (st == REO_CALL_PENDING) {
        return;
    }
    if (st == REO_CALL_DONE) {
        evt(v0 == 0 ? "[evt] reload: combine started" : "[evt] reload: the game refused the combine", 1, (s32)v0);
    } else {
        evt("[evt] reload: the call expired", 0, 0);
    }
    s.reloadTicket = 0;
}

static void driver_init(void) {
    s.init = 1;
    s.nearScale = 1.0f;
    s.camT = 1.0f;
    s.revealT = 1.0f;
    s.captureOn = 1; // the exe starts with mouse capture on
    s.cutForced = s.cutCand = -1;
    s.status = REO_CAM_STATUS_OFF;
    mc_follow_init(&s.follow);
}

static void driver_frame(void) {
    Mc_Config cfg;
    ReoCamInput in;
    ReoCrosshair xh;
    Mc_Rig hipRig, aimRig, eff;
    Mc_Vec3 eye, look;
    u32 frames = Mc_ReadU32(REO_US_FRAMES);
    u32 pressed, held, bound, aiming, aimReal, aimingNow, inputMoving, looking, kbm, haveInput;
    u32 pressBits, suppBits, hideActs, sprintHeld, fireBit, dashBit, writeFov, active, steering, aimSteer;
    u32 freezeState, rate, centreL, centreR;
    f32 dt, px, py, pz, playerYaw, baseYaw, viewYaw, lx, ly, rx, ry, mdx, mdy, behind;
    f32 pdir, ydir, msens, lookScale, aimTrim, gunLim, aimZero, plo, phi, dyaw, mag, blend;

    rate = reo_vblank_rate_mhz();
    s.vblankHz = (rate >= 30000u && rate <= 70000u) ? (f32)rate / 1000.0f : 59.94f;
    if (!s.init) {
        driver_init();
        s.prevFrames = frames;
        return;
    }
    dt = (f32)(frames - s.prevFrames) / s.vblankHz;
    s.prevFrames = frames;
    dt = mc_clampf(dt, 0.0f, MC_MAX_DT);
    s.now += dt;
    s.framesSincePadMap++;

    mc_config_read(&cfg);
    s.debugLog = cfg.debugLog;
    if (!cfg.enabled) {
        if (s.levelLive || s.frozen || s.freeAimOn || s.status != REO_CAM_STATUS_OFF) {
            hand_back_all();
        }
        s.status = REO_CAM_STATUS_OFF;
        s.statusDetail = 0;
        send_status();
        return;
    }
    mc_set_player_slot(reo_local_player_slot());
    if (!s.levelLive) {
        mc_set_view_slot(mc_player_slot());
    }
    s.statusDetail = 0;

    // ---- Level live? (game.bin loaded and her record populated)
    if (!mc_game_loaded() || !mc_level_ready()) {
        if (s.levelLive) {
            if (s.cutForced >= 0) {
                mc_release_cut();
            }
            s.cutForced = s.cutCand = -1;
            s.haveCutEyes = 0;
            s.haveLastWritten = 0;
            s.haveSteerYaw = 0;
            s.cineYield = 0;
            mc_walls_invalidate();
            release_reload();
            s.levelLive = 0;
            mc_set_view_slot(mc_player_slot());
            evt("[evt] no level: idling, camera and pad handed back", 0, 0);
        }
        unfreeze(0);
        input_neutral();
        crosshair_off();
        s.status = REO_CAM_STATUS_WAITING;
        send_status();
        return;
    }
    if (!s.levelLive) {
        u32 st = mc_feature_state(MC_FEAT_FREEZE);

        // The game code must carry the camera's feature sites; without them the game's handlers would fight it.
        if (st == REO_FEATURE_UNAVAILABLE || st == REO_FEATURE_UNKNOWN) {
            if (!s.unavailable) {
                evt("[evt] the game code has no camera.freeze sites: rebuild it with patches.json", 0, 0);
            }
            s.unavailable = 1;
            input_neutral();
            crosshair_off();
            s.status = REO_CAM_STATUS_UNAVAILABLE;
            send_status();
            return;
        }
        s.unavailable = 0;
        freeze();
        reset_rig_to_player();
        s.levelLive = 1;
        s.status = REO_CAM_STATUS_DRIVING;
        send_status();
        evt("[evt] level live: rig seeded from her position", 0, 0);
        return; // seed on this frame, drive from the next
    }
    if (dt <= 0.0f) {
        return; // same game frame as last time
    }

    // The game code must carry the camera's feature sites; without them the game's handlers would fight the camera.
    freezeState = s.cineYield ? REO_FEATURE_OFF : mc_feature(MC_FEAT_FREEZE, s.frozen);
    if (freezeState == REO_FEATURE_UNAVAILABLE || freezeState == REO_FEATURE_UNKNOWN) {
        if (!s.unavailable) {
            evt("[evt] the game code has no camera.freeze sites: rebuild it with patches.json", 0, 0);
        }
        s.unavailable = 1;
        hand_back_all();
        s.status = REO_CAM_STATUS_UNAVAILABLE;
        send_status();
        return;
    }
    s.unavailable = 0;

    sync_engine_state(&cfg);
    poll_reload();

    // ---- Shoulder side (animated swap)
    if (!s.haveShoulder) {
        s.haveShoulder = 1;
        s.shoulderTarget = cfg.hip.shoulder < 0.0f ? -1.0f : 1.0f;
        s.shoulderSign = s.shoulderTarget;
    }
    if (s.shoulderSign != s.shoulderTarget) {
        s.shoulderSign += (s.shoulderTarget - s.shoulderSign) *
                          mc_smooth_alpha(mc_clampf(cfg.shoulderSwapTime, 0.02f, 1.0f), dt);
        if (mc_absf(s.shoulderTarget - s.shoulderSign) < 0.001f) {
            s.shoulderSign = s.shoulderTarget;
        }
    }
    if (!Mc_IsFiniteF32(s.pitch)) {
        s.pitch = 0.0f;
    }
    if (!Mc_IsFiniteF32(s.camYaw)) {
        s.camYaw = mc_view_yaw_deg() + 180.0f;
    }

    // ---- Input: the camera's own device (host), binds as camera actions
    haveInput = reo_cam_input(&in) && in.version == REO_CAM_INPUT_VERSION;
    if (!haveInput) {
        // No camera input service: look and move from the game's own pad frames, no binds.
        Mc_Pad pad;

        mc_pad_read(&pad);
        in.device = REO_CAM_DEVICE_PAD;
        in.held = in.bound = 0;
        in.lx = pad.lx;
        in.ly = pad.ly;
        in.rx = pad.rx;
        in.ry = pad.ry;
        in.mouseDx = in.mouseDy = 0;
        in.focused = 1;
        in.uiOpen = 0;
        if (pad.held & REO_SCE_RIGHT) in.lx += 1.0f;
        if (pad.held & REO_SCE_LEFT) in.lx -= 1.0f;
        if (pad.held & REO_SCE_UP) in.ly += 1.0f;
        if (pad.held & REO_SCE_DOWN) in.ly -= 1.0f;
    }
    kbm = in.device == REO_CAM_DEVICE_KBM;
    held = in.held & in.bound;
    bound = in.bound;
    lx = mc_clampf(in.lx, -1.0f, 1.0f);
    ly = mc_clampf(in.ly, -1.0f, 1.0f);
    rx = mc_clampf(in.rx, -1.0f, 1.0f);
    ry = mc_clampf(in.ry, -1.0f, 1.0f);
    mdx = kbm ? (f32)in.mouseDx : 0.0f;
    mdy = kbm ? (f32)in.mouseDy : 0.0f;
    if (in.uiOpen) {
        rx = ry = mdx = mdy = 0.0f;
    }
    pressed = held & ~s.prevActs;
    s.prevActs = held;

    if (pressed & REO_CAM_ACT_CAPTURE) {
        s.captureOn = !s.captureOn;
        evt("[evt] mouse-look:", 1, (s32)s.captureOn);
    }
    reo_mouse_capture(kbm && s.captureOn && !in.uiOpen);
    if (kbm && !s.captureOn) {
        mdx = mdy = 0.0f; // the free cursor is not camera look
    }
    if (pressed & REO_CAM_ACT_RECENTER) {
        s.recenterTimer = cfg.recenterTime;
    }
    if (pressed & REO_CAM_ACT_LENS) {
        cycle_lens();
    }
    if (pressed & REO_CAM_ACT_REVEAL) {
        // Start or reverse the reveal blend: toward the room's own camera, or back to ours.
        if (s.revealDir > 0 || (s.revealDir == 0 && s.revealT >= 1.0f)) {
            s.revealDir = -1;
        } else {
            s.revealDir = 1;
        }
    }
    if (pressed & REO_CAM_ACT_SWAP) {
        f32 m = mc_absf(cfg.hip.shoulder);

        s.shoulderTarget = -s.shoulderTarget;
        // The exe writes the new side into its settings (update_settings).
        reo_config_set_float("shoulder", f2bits(m * s.shoulderTarget));
        evt(s.shoulderTarget > 0.0f ? "[evt] shoulder: right" : "[evt] shoulder: left", 0, 0);
    }
    if (pressed & REO_CAM_ACT_ALTVIEW) {
        s.altView = !s.altView;
        evt("[evt] alt view:", 1, (s32)s.altView);
    }
    inputMoving = mc_absf(lx) > 0.1f || mc_absf(ly) > 0.1f;

    // ---- Aim
    aiming = (held & REO_CAM_ACT_AIM) != 0;
    aimReal = aiming;
    blend = aimReal ? cfg.aimBlendIn : cfg.aimBlendOut;
    if (!(blend > 0.0f)) {
        blend = cfg.aimBlend; // the exe's aim_blend fallback
    }
    s.aimAmt += ((aimReal ? 1.0f : 0.0f) - s.aimAmt) * mc_smooth_alpha(mc_clampf(blend, 0.02f, 1.0f), dt);
    aimingNow = s.aimAmt > 0.1f;

    // ---- Waiting out a cinematic
    if (s.cineYield) {
        s.status = REO_CAM_STATUS_YIELDED;
        if (cinematic_wait(&cfg, lx, ly)) {
            send_status();
            return;
        }
        s.status = REO_CAM_STATUS_DRIVING;
    }

    // ---- The followed character (the local player, or the watched one)
    mc_view_pos(&px, &py, &pz);
    playerYaw = mc_view_yaw_deg();

    // ---- Did the engine take the camera back? (doors, scripted shots)
    if (check_camera_taken(&cfg)) {
        s.status = REO_CAM_STATUS_YIELDED;
        send_status();
        return;
    }

    // ---- Look: right stick and mouse
    pdir = cfg.invertPitch ? -1.0f : 1.0f;
    ydir = cfg.invertStick ? -1.0f : 1.0f;
    msens = cfg.mouseSensitivity * 0.4f;
    lookScale = 1.0f;
    if (s.aimAmt > 0.001f) {
        lookScale = mc_lerp(1.0f, mc_clampf(cfg.aimSensitivity, 0.05f, 3.0f), s.aimAmt);
    }
    s.pitch -= ry * pdir * cfg.pitchSpeed * dt * lookScale;
    s.pitch += mdy * msens * pdir * lookScale;

    mc_config_presets(&cfg, s.altView, &hipRig, &aimRig);
    aimTrim = (s.aimAmt > 0.001f) ? aimRig.pitchTrim : 0.0f;
    gunLim = mc_clampf(cfg.gunPitchMax, 1.0f, 45.0f);
    aimZero = mc_clampf(cfg.aimZeroDeg, -gunLim, gunLim);
    plo = cfg.pitchMin;
    phi = cfg.pitchMax;
    if (s.aimAmt > 0.001f && cfg.modernAim && cfg.aimPitchLock) {
        // While aiming, the camera cannot tilt further than the gun can follow.
        f32 base = aimZero - aimTrim;

        plo = mc_lerp(plo, base - gunLim, s.aimAmt);
        phi = mc_lerp(phi, base + gunLim, s.aimAmt);
    }
    s.pitch = mc_maxf(plo, mc_minf(phi, s.pitch));

    looking = mc_absf(rx) > MC_LOOK_THRESH || mc_absf(mdx) > 0.001f;
    dyaw = (-(rx * ydir) * cfg.stickSpeed * dt - mdx * ydir * msens) * lookScale;
    s.camYaw += dyaw;
    s.lookOff = mc_angwrap(s.lookOff + dyaw);

    // ---- Buttons the camera presses for the game or takes away from it
    pressBits = suppBits = 0;
    hideActs = 0;
    centreL = centreR = 0;
    dashBit = mc_button_for_action(REO_ACT_DASH);
    if (!dashBit) {
        dashBit = REO_SCE_CIRCLE;
    }

    if (!in.uiOpen) {
        // Special action: press the dash action's button while held.
        if (held & REO_CAM_ACT_SPECIAL) {
            pressBits |= dashBit;
        }

        // Sprint: takes the native dash button away and presses it while the toggle is on.
        sprintHeld = (held & REO_CAM_ACT_SPRINT) != 0;
        if (bound & REO_CAM_ACT_SPRINT) {
            suppBits |= dashBit;
            if (cfg.sprintToggle) {
                if (sprintHeld && !s.prevSprint) {
                    s.sprintOn = !s.sprintOn;
                }
                if (s.wasMoving && !inputMoving) {
                    s.sprintOn = 0;
                }
            } else {
                s.sprintOn = sprintHeld;
            }
            if (s.sprintOn && inputMoving) {
                pressBits |= dashBit;
            }
        } else {
            s.sprintOn = 0;
        }
        s.wasMoving = inputMoving;
        s.prevSprint = sprintHeld;
    }

    // Reload: a press starts it (she lowers the gun first); it runs once the aim stance has ended.
    {
        u32 rHeld = (held & REO_CAM_ACT_RELOAD) != 0;
        u32 cancel = (aiming && !s.prevAimBind) || (inputMoving && !s.prevMove);

        if (cancel && (s.reloadDropsAim || s.reloadPending)) {
            s.reloadDropsAim = 0;
            s.reloadPending = 0;
        }
        s.prevAimBind = aiming;
        s.prevMove = inputMoving;
        if (s.reloadDropsAim && !s.reloadPending) {
            if (s.reloadGrace > 0.0f) {
                s.reloadGrace = mc_maxf(0.0f, s.reloadGrace - dt);
            } else if (!mc_busy_with_action()) {
                s.reloadDropsAim = 0;
            }
        }
        if ((pressed & REO_CAM_ACT_RELOAD) && rHeld) {
            Mc_ReloadPlan plan;

            if (mc_busy_with_action()) {
                evt("[evt] reload: ignored, a combine is already running", 0, 0);
            } else if (!cfg.reloadAnimated || !mc_find_reload(&plan)) {
                evt("[evt] reload: nothing to do - magazine full, or no matching ammo carried", 0, 0);
            } else {
                s.reloadDropsAim = 1;
                s.reloadPending = 1;
                s.reloadPendingT = 0.0f;
            }
        }
        if (s.reloadPending) {
            s.reloadPendingT += dt;
            if (!mc_in_aim_state()) {
                s.reloadPending = 0;
                do_reload(&cfg);
                s.reloadGrace = MC_RELOAD_GRACE;
            } else if (s.reloadPendingT > MC_RELOAD_WAIT) {
                s.reloadPending = 0;
                s.reloadDropsAim = 0;
                evt("[evt] reload: gave up waiting for the aim state to end", 0, 0);
            }
        }
        if (s.reloadDropsAim) {
            centreL = 1;
            s.statusDetail |= REO_CAM_DETAIL_RELOAD_PENDING;
        }
    }

    // Shoot: fires through whichever button fires under the player's control type; while aiming the game's own fire
    // button is taken away.
    if ((bound & REO_CAM_ACT_SHOOT) && !in.uiOpen) {
        fireBit = mc_button_for_action(REO_ACT_FIRE);
        if (!fireBit) {
            fireBit = REO_SCE_CROSS;
        }
        if (aimReal) {
            suppBits |= fireBit;
        }
        if (held & REO_CAM_ACT_SHOOT) {
            pressBits |= fireBit;
        }
    }

    // Modern aim: hold the game's aim button (R1) while the Aim bind is held, and put the gun where the camera looks.
    if (cfg.modernAim && aiming && !s.reloadDropsAim) {
        f32 elev = mc_clampf(-(s.pitch + aimTrim) + aimZero, -gunLim, gunLim);

        pressBits |= REO_SCE_R1;
        if (s.freeAimOn && aimReal) {
            mc_set_aim_angle_deg(elev);
        }
    }
    if (s.aimWas && !aimReal && s.freeAimOn) {
        mc_set_aim_angle_deg(0.0f);
    }
    s.aimWas = aimReal;

    // The camera's own binds never also reach the game while held.
    hideActs = held;
    suppBits &= ~pressBits; // press beats suppress

    // Aim-walk: with the gun's servo off the left stick moves her instead of tilting the gun, and the game must not
    // also see it.
    if (aimReal && s.freeAimOn) {
        centreL = 1;
        if (cfg.aimStrafeSpeed > 0.0f) {
            mc_aim_move_step(lx, ly, dt, cfg.aimStrafeSpeed);
        }
    }
    // The right stick is the game's ad-lib control: without this every camera turn blurts a line (not on Keyboard &
    // Mouse, as the exe).
    centreR = cfg.ownLookStick && !kbm;

    // The pad transform, applied at the camera.padMap mark (recomp_on_pad_map) in this game frame.
    s.map.press = pressBits;
    s.map.supp = suppBits;
    s.map.centreL = centreL;
    s.map.centreR = centreR;
    s.map.active = 1;
    reo_cam_input_hide(hideActs);
    reo_pad_override((centreL ? REO_PAD_CENTRE_LEFT : 0) | (centreR ? REO_PAD_CENTRE_RIGHT : 0));
    if (s.framesSincePadMap > MC_PADMAP_MISSING) {
        // The host has not called recomp_on_pad_map: v1's per-frame write (it can race the game's pad read).
        if (!s.padMapFallbackLogged) {
            s.padMapFallbackLogged = 1;
            evt("[evt] no pad-map event from the host: applying the pad transform once per frame", 0, 0);
        }
        mc_padmap_apply(mc_padmap_player_word(), s.map.press, s.map.supp, s.map.centreL, s.map.centreR);
    }

    // ---- Yaw: follow, free-look return, aim snap, recenter
    behind = cfg.followBehind ? 180.0f : 0.0f;
    s.follow.moveHold = mc_maxf(0.0f, cfg.followHold);
    active = mc_follow_update(&s.follow, px, pz, playerYaw, dt, inputMoving, aimingNow, &baseYaw);
    if (!looking && !(aimingNow && cfg.aimFreeLook) && cfg.keepBehind) {
        mc_release_free_look(&s.camYaw, &s.lookOff, dt, cfg.freeLookReturn);
    }
    viewYaw = mc_angwrap(s.camYaw - behind);
    aimSteer = (aiming || aimingNow) && cfg.modernAim;
    steering = cfg.turnToCamera != MC_TURN_OFF || aimSteer;
    if (active && (cfg.keepBehind || (aimingNow && cfg.aimFollow && !aimSteer)) && !steering) {
        f32 k = aimingNow ? cfg.aimFollowStrength : cfg.followStrength;
        f32 target = baseYaw + behind + cfg.yawOffset + s.lookOff;

        s.camYaw += mc_angwrap(target - s.camYaw) * mc_smooth_alpha(mc_maxf(0.02f, k), dt);
    }
    if (aimReal && !s.aimPrev && cfg.aimSnapBehind) {
        s.lookOff = 0.0f;
    }
    if (cfg.aimSnapBehind && aimingNow && !aimSteer) {
        f32 snapTarget = playerYaw + behind + cfg.yawOffset + s.lookOff;

        if (mc_absf(mc_angwrap(snapTarget - s.camYaw)) > 0.5f) {
            s.camYaw += mc_angwrap(snapTarget - s.camYaw) *
                        mc_smooth_alpha(mc_clampf(cfg.aimSnapSpeed, 0.1f, 1.0f), dt);
        }
    }
    s.aimPrev = aimReal;
    if (s.recenterTimer > 0.0f) {
        f32 a = mc_minf(1.0f, dt * 15.0f);

        s.recenterTimer -= dt;
        s.camYaw += mc_angwrap(playerYaw + behind - s.camYaw) * a;
        s.lookOff = mc_lerp(s.lookOff, 0.0f, a);
        s.pitch = mc_lerp(s.pitch, 0.0f, a);
    }

    // ---- Rig: blend hip -> aim
    writeFov = 1;
    if (hipRig.fovDeg <= 0.0f && aimRig.fovDeg <= 0.0f) {
        writeFov = 0; // 0 = leave the game's FOV alone
    } else {
        f32 engineFov = mc_read_fov_deg();

        if (hipRig.fovDeg <= 0.0f) hipRig.fovDeg = engineFov;
        if (aimRig.fovDeg <= 0.0f) aimRig.fovDeg = engineFov;
    }
    mc_blend_rig(&hipRig, &aimRig, s.aimAmt, &eff);
    eff.lookDrop *= 1.0f - s.aimAmt;
    mag = mc_absf(eff.shoulder);
    if (s.aimAmt > 0.001f && cfg.aimShoulderMin > 0.0f) {
        mag = mc_lerp(mag, mc_maxf(mag, cfg.aimShoulderMin), s.aimAmt);
    }
    eff.shoulder = mag * s.shoulderSign;
    if (writeFov && eff.fovDeg > 0.0f) {
        mc_write_fov_deg(eff.fovDeg);
    }
    if (cfg.fovScale > 0.0f) {
        mc_write_aspect(cfg.fovScale);
        s.aspectOn = 1;
    } else if (s.aspectOn) {
        mc_restore_aspect();
        s.aspectOn = 0;
    }

    // ---- Eye and look-at
    mc_eye_and_look(px, py, pz, s.camYaw, s.pitch + eff.pitchTrim, &eff, &eye, &look);

    // ---- Collision: pull the eye in front of walls
    if (cfg.camCollision) {
        f32 ex = eye.x, ez = eye.z, ey = eye.y;
        f32 tHit;
        u32 ignore = cfg.camIgnoreProps ? REO_WALL_PROP_FLAG_FILE1 : 0;

        mc_walls_refresh(dt);
        // Only the fraction of the boom is used; smoothing the point would drag the eye through walls.
        tHit = mc_clamp_eye_to_walls(&ex, &ez, &ey, look.x, look.z, cfg.camMargin, ignore, 1, look.y, 1,
                                     look.y + cfg.camBlockTop);
        if (tHit < 0.999f) {
            s.camHoldTimer = cfg.camHold;
        } else {
            s.camHoldTimer = mc_maxf(0.0f, s.camHoldTimer - dt);
        }
        if (tHit < s.camT) {
            if (cfg.camSnap > 0.0f) {
                s.camT += (tHit - s.camT) * mc_smooth_alpha(mc_clampf(cfg.camSnap, 0.5f, 1.0f), dt);
            } else {
                s.camT = tHit;
            }
        } else if (s.camHoldTimer > 0.0f) {
            // hold the pulled-in boom
        } else {
            s.camT += (tHit - s.camT) * mc_smooth_alpha(mc_maxf(0.02f, cfg.camRecover), dt);
        }
        if (s.camT < 0.999f) {
            f32 ey2 = look.y + (eye.y - look.y) * s.camT;

            eye.x = look.x + (eye.x - look.x) * s.camT;
            eye.y = mc_maxf(ey2, py + 25.0f);
            eye.z = look.z + (eye.z - look.z) * s.camT;
        }
    }

    // ---- Reveal camera: a smoothstep blend between the room's own cut and the rig
    if (s.revealDir != 0 || s.revealT < 1.0f) {
        if (s.revealDir != 0) {
            f32 span = mc_maxf(0.05f, cfg.revealTime);

            s.revealT = mc_minf(1.0f, mc_maxf(0.0f, s.revealT + (f32)s.revealDir * dt / span));
            if (s.revealT <= 0.0f || s.revealT >= 1.0f) {
                s.revealDir = 0;
            }
        }
        if (s.revealT < 1.0f) {
            Mc_Vec3 fe, ft;
            f32 ffov;

            if (mc_cut_pose(&fe, &ft, &ffov)) {
                f32 e = s.revealT * s.revealT * (3.0f - 2.0f * s.revealT);

                eye.x = fe.x + (eye.x - fe.x) * e;
                eye.y = fe.y + (eye.y - fe.y) * e;
                eye.z = fe.z + (eye.z - fe.z) * e;
                look.x = ft.x + (look.x - ft.x) * e;
                look.y = ft.y + (look.y - ft.y) * e;
                look.z = ft.z + (look.z - ft.z) * e;
            }
        }
    }

    // ---- Write the camera
    mc_write_camera(&eye, &look);

    // ---- Borrow the room cut nearest our eye, so visibility and culling match what we show
    cut_match(&cfg, &eye);
    s.haveLastWritten = 1;
    s.lastWritten = eye;

    // ---- Crosshair (the exe's overlay: AIM 1 while aiming)
    xh.visible = cfg.crosshair && aimingNow;
    xh.style = cfg.crosshairStyle ? REO_XHAIR_CROSS : REO_XHAIR_DOT;
    xh.size = cfg.crosshairSize;
    xh.gap = cfg.crosshairGap;
    xh.thickness = cfg.crosshairThickness;
    xh.rgb = cfg.crosshairRgb;
    xh.dx = cfg.crosshairDx;
    xh.dy = cfg.crosshairDy;
    xh.aimAmount = s.aimAmt;
    reo_cam_crosshair(&xh);

    // ---- Clip values
    write_clip_planes(&cfg);

    // ---- Turn her toward the camera
    if (steering && (aiming || aimingNow) && !aimSteer) {
        steering = 0;
        s.haveSteerYaw = 0;
    }
    if (steering && in.uiOpen) {
        steering = 0;
        s.haveSteerYaw = 0;
    }
    if (steering && mc_view_slot() != mc_player_slot()) {
        steering = 0; // watching another player: her heading stays hers (the view is not hers)
        s.haveSteerYaw = 0;
    }
    if (steering) {
        u32 pushing;
        f32 targetYaw, gap;

        if (aimSteer) {
            pushing = mc_absf(rx) > MC_LOOK_THRESH ||
                      mc_absf(mc_angwrap(viewYaw - playerYaw)) > cfg.modernAimDeadzone;
            targetYaw = viewYaw;
        } else if (cfg.turnToCamera == MC_TURN_ALWAYS) {
            pushing = 1;
            targetYaw = viewYaw;
        } else {
            pushing = mc_compute_modern_movement(lx, ly, viewYaw, playerYaw, MC_MOVE_DEADZONE, &targetYaw, &gap);
        }
        if (pushing) {
            f32 step = mc_maxf(1.0f, cfg.turnSpeed) * dt;
            f32 live;

            if (!s.haveSteerYaw) {
                s.haveSteerYaw = 1;
                s.steerYaw = mc_player_heading_deg();
            }
            gap = mc_angwrap(targetYaw - s.steerYaw);
            s.steerYaw = mc_angwrap(s.steerYaw + mc_clampf(gap, -step, step));
            live = mc_player_heading_deg();
            if (mc_absf(mc_angwrap(s.steerYaw - live)) >= MC_HEADING_EPS) {
                mc_set_player_heading(s.steerYaw);
            }
        } else {
            s.haveSteerYaw = 0;
        }
    } else {
        s.haveSteerYaw = 0;
    }

    if (s.altView) {
        s.statusDetail |= REO_CAM_DETAIL_ALT_VIEW;
    }
    if (mc_view_slot() != mc_player_slot()) {
        s.statusDetail |= REO_CAM_DETAIL_WATCHING_OTHER;
    }
    s.status = REO_CAM_STATUS_DRIVING;
    send_status();
}

// The game-frame event (the frame mark, exe 0x0019F6C0).
RECOMP_CALLBACK("*", recomp_on_play_main)
void Mcam_OnPlayMain(void) {
    driver_frame();
}

// The camera.padMap mark (exe 0x001B0C5C): the mapper is about to load the held word at heldWordAddr for the
// character record charRecord. ModernCam's VBTN cave: `t4 = (t4 & KEEPMASK) | PRESS` and the stick groups, applied
// here to her record only, exactly where the game reads them.
RECOMP_CALLBACK("*", recomp_on_pad_map)
void Mcam_OnPadMap(u32 heldWordAddr, u32 charRecord) {
    s.padMapCalls++;
    s.framesSincePadMap = 0;
    if (!s.map.active || !s.levelLive || s.cineYield) {
        return;
    }
    if (heldWordAddr != mc_padmap_player_word() || charRecord != mc_char_base(mc_player_slot())) {
        return;
    }
    mc_padmap_apply(heldWordAddr, s.map.press, s.map.supp, s.map.centreL, s.map.centreR);
}
