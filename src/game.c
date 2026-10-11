#include "mcam_game.h"
#include "mcam_mem.h"
#include "reo_camera_us.h"
#include "reo_host.h"

// Ported from Outbreak ModernCam's ro_addresses.py (File 1). Widths, offsets and write order follow
// the original; the addresses are this disc's (reo_camera_us.h). v2: data only - the camera freeze and free aim are
// the host's feature switches (patches.json), never code writes.

// ---- Feature switches ------------------------------------------------------------------------------------------

static const char* const kFeatureNames[MC_FEAT_COUNT] = { REO_FEATURE_CAMERA_FREEZE, REO_FEATURE_CAMERA_FREEAIM };
static u32 sFeatureSent[MC_FEAT_COUNT] = { 2, 2 }; // 2 = nothing sent yet
static u32 sFeatureState[MC_FEAT_COUNT] = { REO_FEATURE_OFF, REO_FEATURE_OFF };

u32 mc_feature(u32 which, u32 on) {
    on = on ? 1 : 0;
    if (which >= MC_FEAT_COUNT) {
        return REO_FEATURE_UNKNOWN;
    }
    if (sFeatureSent[which] != on) {
        sFeatureState[which] = reo_feature_set(kFeatureNames[which], on);
        sFeatureSent[which] = on;
    }
    return sFeatureState[which];
}

u32 mc_feature_state(u32 which) {
    if (which >= MC_FEAT_COUNT) {
        return REO_FEATURE_UNKNOWN;
    }
    sFeatureState[which] = reo_feature_state(kFeatureNames[which]);
    return sFeatureState[which];
}

void mc_feature_forget(void) {
    u32 i;

    for (i = 0; i < MC_FEAT_COUNT; i++) {
        sFeatureSent[i] = 2;
    }
}

u32 mc_game_loaded(void) {
    return reo_image_loaded("game.bin");
}

// ---- Characters ------------------------------------------------------------------------------------------------

static u32 sPlayerSlot = 0;
static u32 sViewSlot = 0;

void mc_set_player_slot(u32 slot) {
    sPlayerSlot = slot & 3;
}

u32 mc_player_slot(void) {
    return sPlayerSlot;
}

void mc_set_view_slot(u32 slot) {
    sViewSlot = slot & 3;
}

u32 mc_view_slot(void) {
    return sViewSlot;
}

u32 mc_char_base(u32 slot) {
    return REO_US_CHAR_BASE + (slot & 3) * REO_US_CHAR_STRIDE;
}

static void char_pos(u32 slot, f32* x, f32* y, f32* z) {
    u32 base = mc_char_base(slot) + REO_CHAR_POS;

    *x = Mc_ReadF32(base);
    *y = Mc_ReadF32(base + 4);
    *z = Mc_ReadF32(base + 8);
}

static f32 char_yaw_deg(u32 slot) {
    u32 base = mc_char_base(slot);
    f32 fx = Mc_ReadF32(base + REO_CHAR_FWD_X);
    f32 fz = Mc_ReadF32(base + REO_CHAR_FWD_Z);
    f32 n = mc_hypotf(fx, fz);

    if (!(n >= 1e-6f) || !Mc_IsSaneF32(n)) {
        return 0.0f; // atan2(0, 1)
    }
    return mc_atan2f(fx / n, fz / n) * MC_RAD2DEG;
}

u32 mc_char_live(u32 slot) {
    // A level is loaded and the record is live: the follow camera must not seed its smoothers on an empty struct.
    f32 x, y, z;

    char_pos(slot, &x, &y, &z);
    if (!Mc_IsSaneF32(x) || !Mc_IsSaneF32(y) || !Mc_IsSaneF32(z)) {
        return 0;
    }
    return x != 0.0f || y != 0.0f || z != 0.0f;
}

u32 mc_level_ready(void) {
    return mc_char_live(sPlayerSlot);
}

void mc_view_pos(f32* x, f32* y, f32* z) {
    char_pos(sViewSlot, x, y, z);
}

f32 mc_view_yaw_deg(void) {
    return char_yaw_deg(sViewSlot);
}

void mc_player_pos(f32* x, f32* y, f32* z) {
    char_pos(sPlayerSlot, x, y, z);
}

void mc_set_player_pos(f32 x, f32 y, f32 z) {
    u32 base = mc_char_base(sPlayerSlot) + REO_CHAR_POS;

    Mc_WriteF32(base, x);
    Mc_WriteF32(base + 4, y);
    Mc_WriteF32(base + 8, z);
}

f32 mc_player_yaw_deg(void) {
    return char_yaw_deg(sPlayerSlot);
}

f32 mc_player_heading_deg(void) {
    return (f32)Mc_ReadS16(mc_char_base(sPlayerSlot) + REO_CHAR_HEADING) * (360.0f / 65536.0f);
}

void mc_set_player_heading(f32 deg) {
    s32 v = (s32)mc_floorf(mc_fmodpf(deg, 360.0f) * (65536.0f / 360.0f) + 0.5f) & 0xFFFF;

    Mc_WriteU16(mc_char_base(sPlayerSlot) + REO_CHAR_HEADING, (u32)v);
}

void mc_set_aim_angle_deg(f32 deg) {
    s32 v = (s32)mc_floorf(deg * (65536.0f / 360.0f) + 0.5f);

    if (v > REO_AIM_ANGLE_MAX) {
        v = REO_AIM_ANGLE_MAX;
    } else if (v < -REO_AIM_ANGLE_MAX) {
        v = -REO_AIM_ANGLE_MAX;
    }
    Mc_WriteU16(mc_char_base(sPlayerSlot) + REO_CHAR_AIM_ANGLE, (u32)v & 0xFFFF);
}

u32 mc_in_aim_state(void) {
    return Mc_ReadU8(mc_char_base(sPlayerSlot) + REO_CHAR_STANCE) == 2;
}

u32 mc_aim_target(Mc_Vec3* pos) {
    u32 base = mc_char_base(sPlayerSlot);
    u32 stance = Mc_ReadU8(base + REO_CHAR_STANCE);
    u32 t;

    // Aiming, or hit while aiming, with the aim action still held: the game keeps the target it chose.
    if ((stance != REO_STANCE_AIM && stance != REO_STANCE_AIM_HIT) ||
        !(Mc_ReadU32(base + REO_CHAR_ACT + 8) & REO_ACT_AIM)) {
        return 0;
    }
    t = Mc_ReadU32(base + REO_CHAR_AIM_TARGET);
    // An enemy record (never a stray word): inside the finder's table, on a record boundary.
    if (t < REO_US_ENEMY_BASE || t >= REO_US_CHAR_BASE || (t - REO_US_ENEMY_BASE) % REO_US_ENEMY_STRIDE != 0) {
        return 0;
    }
    if (Mc_ReadU8(t + REO_ENEMY_LIVE) == 0 || Mc_ReadU32(t + REO_ENEMY_MODEL) == 0 ||
        (Mc_ReadU32(t + REO_ENEMY_FLAGS) & REO_ENEMY_UNTARGETABLE)) {
        return 0;
    }
    pos->x = Mc_ReadF32(t + REO_ENEMY_POS);
    pos->y = Mc_ReadF32(t + REO_ENEMY_POS + 4);
    pos->z = Mc_ReadF32(t + REO_ENEMY_POS + 8);
    return Mc_IsSaneF32(pos->x) && Mc_IsSaneF32(pos->y) && Mc_IsSaneF32(pos->z);
}

u32 mc_player_move_held(void) {
    return (Mc_ReadU32(mc_char_base(sPlayerSlot) + REO_CHAR_ACT + 8) & REO_ACT_MOVE) != 0;
}

u32 mc_busy_with_action(void) {
    return Mc_ReadU8(mc_char_base(sPlayerSlot) + REO_CHAR_SUBSTATE) == 0x40;
}

f32 mc_aim_move_step(f32 right, f32 forward, f32 dt, f32 speed) {
    // File 1 roots her when she aims; with the gun's servo silenced the left stick is free, so she
    // is moved in her own frame (+right = her right, +forward = her facing). The engine still
    // resolves her against the world afterwards.
    f32 r = mc_clampf(right, -1.0f, 1.0f);
    f32 f = mc_clampf(forward, -1.0f, 1.0f);
    f32 mag, yaw, step, dx, dz, x, y, z;

    if (r == 0.0f && f == 0.0f) {
        return 0.0f;
    }
    mag = mc_hypotf(r, f);
    if (mag > 1.0f) {
        r /= mag;
        f /= mag;
    }
    yaw = mc_player_heading_deg() * MC_DEG2RAD;
    step = speed * dt;
    // forward = (sin yaw, cos yaw), right = (-cos yaw, sin yaw)
    dx = (-mc_cosf(yaw) * r + mc_sinf(yaw) * f) * step;
    dz = (mc_sinf(yaw) * r + mc_cosf(yaw) * f) * step;
    mc_player_pos(&x, &y, &z);
    mc_set_player_pos(x + dx, y, z + dz);
    return mc_hypotf(dx, dz);
}

// ---- Camera ----------------------------------------------------------------------------------------------------

void mc_write_camera(const Mc_Vec3* eye, const Mc_Vec3* look) {
    Mc_WriteF32(REO_US_CAM_EYE + 0, eye->x);
    Mc_WriteF32(REO_US_CAM_EYE + 4, eye->y);
    Mc_WriteF32(REO_US_CAM_EYE + 8, eye->z);
    Mc_WriteF32(REO_US_CAM_TARGET + 0, look->x);
    Mc_WriteF32(REO_US_CAM_TARGET + 4, look->y);
    Mc_WriteF32(REO_US_CAM_TARGET + 8, look->z);
}

void mc_read_camera_eye(Mc_Vec3* eye) {
    eye->x = Mc_ReadF32(REO_US_CAM_EYE + 0);
    eye->y = Mc_ReadF32(REO_US_CAM_EYE + 4);
    eye->z = Mc_ReadF32(REO_US_CAM_EYE + 8);
}

f32 mc_read_fov_deg(void) {
    f32 v = Mc_ReadF32(REO_US_CAM_FOV_DEG);

    return (Mc_IsSaneF32(v) && v > 0.0f) ? v : 60.0f;
}

void mc_write_fov_deg(f32 deg) {
    Mc_WriteF32(REO_US_CAM_FOV_DEG, deg);
}

void mc_write_near_far(f32 nearClip, f32 farClip) {
    if (nearClip > 0.0f) {
        Mc_WriteF32(REO_US_CAM_NEAR, nearClip);
    }
    if (farClip > 0.0f) {
        Mc_WriteF32(REO_US_CAM_FAR, farClip);
    }
}

f32 mc_read_far(void) {
    return Mc_ReadF32(REO_US_CAM_FAR);
}

static u32 sHaveAspect = 0;
static f32 sAspectOrig;

void mc_write_aspect(f32 scale) {
    if (!sHaveAspect) {
        sAspectOrig = Mc_ReadF32(REO_US_CAM_ASPECT);
        sHaveAspect = 1;
    }
    Mc_WriteF32(REO_US_CAM_ASPECT, scale);
}

void mc_restore_aspect(void) {
    if (sHaveAspect) {
        Mc_WriteF32(REO_US_CAM_ASPECT, sAspectOrig);
        sHaveAspect = 0;
    }
}

// ---- Cuts ------------------------------------------------------------------------------------------------------

u32 mc_cut_count(void) {
    u32 n = Mc_ReadU8(REO_US_CUT_HEADER + REO_CUT_COUNT);

    return (n > 0 && n <= 64) ? n : 0;
}

u32 mc_cut_index(void) {
    return Mc_ReadU8(REO_US_CUT_HEADER + REO_CUT_CURRENT);
}

static void read_ivec(u32 addr, Mc_Vec3* v) {
    // The authored eye and target are three s32, not floats.
    v->x = (f32)(s32)Mc_ReadU32(addr);
    v->y = (f32)(s32)Mc_ReadU32(addr + 4);
    v->z = (f32)(s32)Mc_ReadU32(addr + 8);
}

u32 mc_cut_eye(u32 index, Mc_Vec3* eye) {
    if (index >= mc_cut_count()) {
        return 0;
    }
    read_ivec(REO_US_CUT_HEADER + REO_CUT_RECORDS + index * REO_CUT_STRIDE + REO_CUT_EYE, eye);
    return 1;
}

u32 mc_cut_pose(Mc_Vec3* eye, Mc_Vec3* look, f32* fov) {
    u32 idx = mc_cut_index();
    u32 rec;

    if (idx >= mc_cut_count()) {
        return 0;
    }
    rec = REO_US_CUT_HEADER + REO_CUT_RECORDS + idx * REO_CUT_STRIDE;
    read_ivec(rec + REO_CUT_EYE, eye);
    read_ivec(rec + REO_CUT_TARGET, look);
    *fov = Mc_ReadF32(rec + REO_CUT_FOV);
    return 1;
}

void mc_force_cut(u32 index) {
    // Index first, then the flag.
    Mc_WriteU8(REO_US_CUT_HEADER + REO_CUT_FORCE_IDX, index & 0xFF);
    Mc_WriteU8(REO_US_CUT_HEADER + REO_CUT_FORCE_ON, 1);
}

void mc_release_cut(void) {
    Mc_WriteU8(REO_US_CUT_HEADER + REO_CUT_FORCE_ON, 0);
}

// ---- Visibility and lighting -----------------------------------------------------------------------------------

u32 mc_show_all_objects(void) {
    // Nothing to restore afterwards: the engine rewrites these flags on every cut change.
    u32 n = Mc_ReadU16(REO_US_OBJ_COUNT);
    u32 changed = 0;
    u32 i;

    if (n == 0 || n > 256) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        u32 f = Mc_ReadU8(REO_US_OBJ_FLAGS + i);

        if (!(f & 1)) {
            Mc_WriteU8(REO_US_OBJ_FLAGS + i, f | 1);
            changed++;
        }
    }
    return changed;
}

// Each of the room's fixed cameras (cuts) draws only the objects of its mask. A cut leaves out what stands between its
// lens and the room (the wall it looks through, the pictures on that wall, a bulb next to the lens), and the cut still
// follows her position while the camera drives: those walls and pictures were missing in the view (J's Bar's restroom:
// the far wall and its framed poster, black). Every cut's mask also holds the objects the cuts that look at them draw.
// An object every cut that looks at it leaves out stays out (the restroom's vent grille: the vent is open, its grille
// lies on the floor); so do objects no cut draws and an event's hide (flag bit 7). Some objects no cut draws while the
// camera drives (the drop: J's Bar's restroom, see the mirrors below).
#define MC_MAX_ROOM_CUTS 64      // as mc_cut_count
#define MC_ART_MARGIN_TAN 0.267949f // a cut "looks at" an object whose origin is within its field of view + 15 degrees
static u32 sArtOn = 0;
static u32 sArtCuts, sArtWords;
static u32 sArtAdd[REO_CUT_MASK_WORDS];               // what every cut also draws
static u32 sArtDrop[REO_CUT_MASK_WORDS];              // what no cut draws
static u32 sArtOrig[MC_MAX_ROOM_CUTS][REO_CUT_MASK_WORDS];

// A cut's camera: eye, axes and the tangents of its half fields of view (+ the margin; 0 = no limit).
typedef struct {
    u32 ok;
    Mc_Vec3 eye, fw, rt, up;
    f32 tx, ty;
} Mc_ArtCam;
static Mc_ArtCam sArtCam[MC_MAX_ROOM_CUTS];

static u32 art_mask_addr(u32 cut, u32 w) {
    return REO_US_CUT_HEADER + REO_CUT_RECORDS + cut * REO_CUT_STRIDE + REO_CUT_MASK + 4 * w;
}

static f32 art_tan_add(f32 a, f32 b) {
    f32 d = 1.0f - a * b;

    return d > 0.01f ? (a + b) / d : 0.0f;
}

static void art_cam(u32 cut, Mc_ArtCam* k) {
    u32 rec = REO_US_CUT_HEADER + REO_CUT_RECORDS + cut * REO_CUT_STRIDE;
    Mc_Vec3 tgt;
    f32 fov = Mc_ReadF32(rec + REO_CUT_FOV);
    f32 l, tv;

    read_ivec(rec + REO_CUT_EYE, &k->eye);
    read_ivec(rec + REO_CUT_TARGET, &tgt);
    if (!Mc_IsSaneF32(fov) || fov < 1.0f || fov > 170.0f) {
        fov = 60.0f;
    }
    k->fw.x = tgt.x - k->eye.x;
    k->fw.y = tgt.y - k->eye.y;
    k->fw.z = tgt.z - k->eye.z;
    l = mc_sqrtf(k->fw.x * k->fw.x + k->fw.y * k->fw.y + k->fw.z * k->fw.z);
    k->ok = l > 1e-3f;
    if (!k->ok) {
        return;
    }
    k->fw.x /= l;
    k->fw.y /= l;
    k->fw.z /= l;
    l = mc_hypotf(k->fw.z, k->fw.x);
    k->rt.x = l > 1e-4f ? k->fw.z / l : 1.0f;
    k->rt.y = 0.0f;
    k->rt.z = l > 1e-4f ? -k->fw.x / l : 0.0f;
    k->up.x = k->rt.y * k->fw.z - k->rt.z * k->fw.y;
    k->up.y = k->rt.z * k->fw.x - k->rt.x * k->fw.z;
    k->up.z = k->rt.x * k->fw.y - k->rt.y * k->fw.x;
    fov *= 0.5f * MC_DEG2RAD;
    tv = mc_sinf(fov) / mc_cosf(fov);
    k->ty = art_tan_add(tv, MC_ART_MARGIN_TAN);
    k->tx = art_tan_add(tv * (4.0f / 3.0f), MC_ART_MARGIN_TAN);
}

static u32 art_seen(const Mc_ArtCam* k, const Mc_Vec3* p) {
    f32 rx = p->x - k->eye.x;
    f32 ry = p->y - k->eye.y;
    f32 rz = p->z - k->eye.z;
    f32 z = rx * k->fw.x + ry * k->fw.y + rz * k->fw.z;
    f32 x = mc_absf(rx * k->rt.x + ry * k->rt.y + rz * k->rt.z);
    f32 y = mc_absf(rx * k->up.x + ry * k->up.y + rz * k->up.z);

    if (!(z > 0.0f)) {
        return 0;
    }
    return (k->tx == 0.0f || x <= z * k->tx) && (k->ty == 0.0f || y <= z * k->ty);
}

// A cut's mask while the camera drives.
static u32 art_mask(u32 c, u32 w) {
    return (sArtOrig[c][w] | sArtAdd[w]) & ~sArtDrop[w];
}

// The room's masks are still the ones written (the same room).
static u32 art_ours(void) {
    u32 c, w;

    if (Mc_ReadU8(REO_US_CUT_HEADER + REO_CUT_COUNT) != sArtCuts ||
        (Mc_ReadU16(REO_US_OBJ_COUNT) + 31) / 32 != sArtWords) {
        return 0;
    }
    for (c = 0; c < sArtCuts; c++) {
        for (w = 0; w < sArtWords; w++) {
            if (Mc_ReadU32(art_mask_addr(c, w)) != art_mask(c, w)) {
                return 0;
            }
        }
    }
    return 1;
}

// Bit 0 of the objects whose bit the camera's mask of the current cut changes (another cut draws it, or it is
// dropped): as the camera's mask has it (ours), or as the game's has it.
static void art_current(u32 ours) {
    u32 cur = Mc_ReadU8(REO_US_CUT_HEADER + REO_CUT_CURRENT);
    u32 count = Mc_ReadU16(REO_US_OBJ_COUNT);
    u32 i, f, bit, game, mine, want;

    if (cur >= sArtCuts) {
        return;
    }
    for (i = 0; i < count; i++) {
        bit = 1u << (i & 31);
        game = sArtOrig[cur][i >> 5] & bit;
        mine = art_mask(cur, i >> 5) & bit;
        if (mine == game) {
            continue;
        }
        want = (ours ? mine : game) != 0;
        f = Mc_ReadU8(REO_US_OBJ_FLAGS + i);
        if ((f & 1) != want) {
            Mc_WriteU8(REO_US_OBJ_FLAGS + i, want ? (f | 1) : (f & ~1u));
        }
    }
}

// Every cut's mask as the camera has it.
static void art_write(void) {
    u32 c, w;

    for (c = 0; c < sArtCuts; c++) {
        for (w = 0; w < sArtWords; w++) {
            if (Mc_ReadU32(art_mask_addr(c, w)) != art_mask(c, w)) {
                Mc_WriteU32(art_mask_addr(c, w), art_mask(c, w));
            }
        }
    }
}

static void mirror_drop(u32* drop);
static void mirror_frame(void);
static void glow_restore(void);

static void art_keep(void) {
    u32 n = Mc_ReadU8(REO_US_CUT_HEADER + REO_CUT_COUNT);
    u32 count = Mc_ReadU16(REO_US_OBJ_COUNT);
    u32 c, w, i, any, looked, drawn, same;
    u32 drop[REO_CUT_MASK_WORDS];
    Mc_Vec3 p;

    mirror_drop(drop);
    if (sArtOn && art_ours()) {
        same = 1;
        for (w = 0; w < REO_CUT_MASK_WORDS; w++) {
            same &= drop[w] == sArtDrop[w];
        }
        if (!same) {
            // The same room, another drop: the current cut's objects back as the game's, then the new masks.
            art_current(0);
            for (w = 0; w < REO_CUT_MASK_WORDS; w++) {
                sArtDrop[w] = drop[w];
            }
            art_write();
            art_current(1);
        }
        return;
    }
    sArtOn = 0;
    if (n == 0 || n > MC_MAX_ROOM_CUTS || count == 0 || count > 32 * REO_CUT_MASK_WORDS) {
        return;
    }
    sArtCuts = n;
    sArtWords = (count + 31) / 32;
    for (c = 0; c < n; c++) {
        for (w = 0; w < sArtWords; w++) {
            sArtOrig[c][w] = Mc_ReadU32(art_mask_addr(c, w));
        }
        art_cam(c, &sArtCam[c]);
    }
    // Per object: drawn by a cut that looks at it; one no cut looks at: drawn by any cut.
    for (w = 0; w < REO_CUT_MASK_WORDS; w++) {
        sArtAdd[w] = 0;
    }
    for (i = 0; i < count; i++) {
        u32 m = REO_US_OBJ_MATRIX + REO_OBJ_MATRIX_STRIDE * i + REO_OBJ_MATRIX_ORIGIN;
        u32 sane;

        p.x = Mc_ReadF32(m);
        p.y = Mc_ReadF32(m + 4);
        p.z = Mc_ReadF32(m + 8);
        sane = Mc_IsSaneF32(p.x) && Mc_IsSaneF32(p.y) && Mc_IsSaneF32(p.z);
        any = looked = drawn = 0;
        for (c = 0; c < n; c++) {
            u32 bit = (sArtOrig[c][i >> 5] >> (i & 31)) & 1;

            any |= bit;
            if (sArtCam[c].ok && sane && art_seen(&sArtCam[c], &p)) {
                looked = 1;
                drawn |= bit;
            }
        }
        if (looked ? drawn : any) {
            sArtAdd[i >> 5] |= 1u << (i & 31);
        }
    }
    for (w = 0; w < REO_CUT_MASK_WORDS; w++) {
        sArtDrop[w] = drop[w];
    }
    art_write();
    sArtOn = 1;
    art_current(1); // (the current cut was set with the game's mask)
}

void mc_keep_artwork(void) {
    art_keep();
    mirror_frame();
}

void mc_restore_artwork(void) {
    u32 c, w;

    glow_restore();
    if (!sArtOn) {
        return;
    }
    if (art_ours()) {
        for (c = 0; c < sArtCuts; c++) {
            for (w = 0; w < sArtWords; w++) {
                if (Mc_ReadU32(art_mask_addr(c, w)) != sArtOrig[c][w]) {
                    Mc_WriteU32(art_mask_addr(c, w), sArtOrig[c][w]);
                }
            }
        }
        art_current(0);
    }
    sArtOn = 0;
}

// ---- J's Bar's restroom mirrors -------------------------------------------------------------------------------------
// The two mirrors over the sinks are no mirrors: grimy glass panes (room object 46) in front of a mirrored copy of the
// room's door end and poster wall built behind the wall (objects 14 and 15) for the game's own fixed camera of the
// sinks (cut 0) only; from the camera's angles the copy showed black glass and two floating posters. While the camera
// drives there, the mirrors show the room reflected, as the game's own mirror shows it from its fixed camera:
//   - at the room's draw (recomp_on_room_draw: the game's room loop, before any of its objects; the camera.roomDraw
//     mark of patches.json) every object the frame draws in front of the mirror wall whose reflection the glass can
//     show is drawn once more, its world matrix mirrored about the mirror plane (the glass, z = 13025), with the game's
//     own functions (reo_guest_call: the world matrix and material slots, the render states, the model;
//     reo_camera_us.h) and clipped to the part of the glass panes on the screen (the game's own scissor routine: GS
//     SCISSOR_1 around it, the game's value back right after). The GS renderers fill a primitive's box within the
//     scissor rectangle, so each pane on the screen has a pass with its own rectangle unless one around both is smaller
//     or the two meet. The walls and frames drawn after it hide it outside the glass, and the glass blends over it as
//     it does over the copy. Whether the glass can show an object's reflection: its model's bounds (read from the
//     model's vertices when the camera first drives in the room), mirrored, seen from the eye through the part of a
//     pane on the screen. Not mirrored: what is mounted on the mirror wall (origin nearer the plane than
//     MC_MIR_WALL_DEPTH: the glass, its frames, the light bars and their plates; their reflections lie on themselves,
//     and parts of them reach into the wall, which mirrored would stand out of it next to the glass) and the counter
//     top (38: it reaches more than 10 units into the wall, so mirrored it would lie on the real counter top in front
//     of the wall; its reflection lies below the glass, seen through it only from nearer than about 90 units and higher
//     than 240);
//   - the copy (14, 15) is in no cut while the camera drives there (the artwork's drop, so a cut change never brings it
//     back; Show Hidden Objects shows every object, so the copy's flags are also cleared right before the reflection
//     is drawn);
//   - the light bars' glow cards (below) look from the camera's angles as they look from the fixed camera.
// Characters are not reflected: the game's own mirror never reflected them either. Without the mark (an older game
// build) the copy stays as before. Handed back (mc_restore_artwork), everything is the game's again.
#define MC_MIR_OBJECTS  98      // the restroom: 98 objects, the glass and the copy at their places
#define MC_MIR_COPY     14
#define MC_MIR_POSTERS  15
#define MC_MIR_COUNTER  38      // the counter top
#define MC_MIR_GLASS    46
#define MC_MIR_PLANE    13025.0f // the glass's plane: z (the room on the +z side)
#define MC_MIR_WALL_DEPTH 12.0f // objects whose origin is nearer the plane are on the mirror wall (not mirrored)
#define MC_MIR_PANE_Y0  180.2f  // the two panes (the glass model's corners, world units)
#define MC_MIR_PANE_Y1  241.8f
#define MC_MIR_MARGIN   2.0f    // GS pixels around the panes' outline on the screen (the frames meet the glass there)
#define MC_MIR_CULL_MARGIN 2.0f // world units around a pane's part on the screen: a reflection seen there is drawn
#define MC_MIR_NEAR     1.0f    // the projection's near plane (0x0032417C)
#define MC_MIR_MAX_CALLS 3000   // calls of the game's code in one room draw (the host makes at most 4096)
#define MC_MIR_BOUNDS_STEP 1500 // model vertices read for the bounds in one frame (the rest in the next frames)
static const f32 kMirPaneX[4] = { 9684.7f, 9734.4f, 9850.0f, 9899.7f };
static u32 sMirMarked = 0;      // the game build raises recomp_on_room_draw (seen once)
static u32 sMirCalls;
static f32 sMirPart[2][4];      // per pane: the part of it on the screen, on the plane (world x0, y0, x1, y1)
static u32 sMirSeen[2];         // the pane is on the screen
// The objects' bounds: per object its model's vertices' box (model space), read once per room (the model table), and
// that box in the world (for the matrix whose origin it was made with).
static u32 sBoxTable;           // the model table they were read from (0: none)
static u32 sBoxNext;            // the next object to read
static u32 sBoxOk[REO_CUT_MASK_WORDS];      // bit i: object i's box is known
static u32 sBoxInWorld[REO_CUT_MASK_WORDS]; // bit i: object i's world box is made
static f32 sBox[MC_MIR_OBJECTS][6];         // x0, y0, z0, x1, y1, z1
static f32 sBoxWorld[MC_MIR_OBJECTS][6];
static f32 sBoxAt[MC_MIR_OBJECTS][3];       // the origin (+ the room's offset) the world box was made with
// The frame's plan: per object the panes (bit k: pane k) whose part on the screen the place of its reflection on the
// plane meets (both: no box yet); 0 = not drawn.
static u8 sPlanPanes[MC_MIR_OBJECTS];
// The material slots as this room draw set them last (a slot holding the material is not set again).
static u32 sSlotMat[32];

static u32 mirror_at(u32 obj, f32 x, f32 y, f32 z) {
    u32 m = REO_US_OBJ_MATRIX + REO_OBJ_MATRIX_STRIDE * obj + REO_OBJ_MATRIX_ORIGIN;
    f32 dx = Mc_ReadF32(m) - x;
    f32 dy = Mc_ReadF32(m + 4) - y;
    f32 dz = Mc_ReadF32(m + 8) - z;

    return Mc_IsSaneF32(dx) && Mc_IsSaneF32(dy) && Mc_IsSaneF32(dz) && dx * dx + dy * dy + dz * dz < 0.25f;
}

// J's Bar's restroom: 98 objects, the glass and the copy at their places.
static u32 mirror_room(void) {
    return Mc_ReadU16(REO_US_OBJ_COUNT) == MC_MIR_OBJECTS && mirror_at(MC_MIR_GLASS, 9792.24f, 211.01f, MC_MIR_PLANE) &&
           mirror_at(MC_MIR_COPY, 9490.0f, 250.0f, 12687.5f);
}

// The artwork's drop: in the restroom the copy, when the reflection is drawn.
static void mirror_drop(u32* drop) {
    u32 w;

    for (w = 0; w < REO_CUT_MASK_WORDS; w++) {
        drop[w] = 0;
    }
    if (!mirror_room() || !sMirMarked) {
        return;
    }
    drop[MC_MIR_COPY >> 5] |= 1u << (MC_MIR_COPY & 31);
    drop[MC_MIR_POSTERS >> 5] |= 1u << (MC_MIR_POSTERS & 31);
}

static u32 mirror_in_ram(u32 a, u32 n) {
    return a >= 0x00100000u && a < 0x02000000u && n <= 0x02000000u - a;
}

// ---- The light bars' glow cards -------------------------------------------------------------------------------------
// Each bulb of the restroom's two light bars has a glow card (63, 66, ..., 84): a glowing fin across the wall next to
// its bulb, drawn after its bulb and before the next bulb, writing depth. The game's fixed camera sees the bars from
// their right end: every card from its right, the bulbs drawn after a card in front of it. From the left of a card the
// bulbs drawn after it lie behind its fin, which hid them (transparent, but writing depth): a flat glowing board over
// the bar's next bulbs. While the camera drives there, a card it sees from its left is drawn without writing depth
// (its model entry's depth-write byte, which the game's render states hand on), so the bulbs behind it show through
// its glow as the fixed camera shows them; a card seen from its right is drawn as the game draws it (its fin still
// hides its own small inner card: the fixed camera's glow). Handed back, the bytes are the game's again.
#define MC_GLOW_CARDS 8
static const u8 kGlowCard[MC_GLOW_CARDS] = { 63, 66, 69, 72, 75, 78, 81, 84 };
static u32 sGlowOn = 0;
static u32 sGlowTable;          // the model table the bytes were written in
static u8 sGlowOrig[MC_GLOW_CARDS]; // the game's depth-write bytes
static u8 sGlowMine[MC_GLOW_CARDS]; // as written

static void glow_restore(void) {
    u32 k, a;

    if (!sGlowOn) {
        return;
    }
    if (mirror_room() && Mc_ReadU32(REO_US_ROOM_DRAW + REO_ROOM_MODELS) == sGlowTable) {
        for (k = 0; k < MC_GLOW_CARDS; k++) {
            a = sGlowTable + REO_MODEL_STRIDE * kGlowCard[k] + REO_MODEL_ZWRITE;
            if (Mc_ReadU8(a) == sGlowMine[k] && sGlowMine[k] != sGlowOrig[k]) {
                Mc_WriteU8(a, sGlowOrig[k]);
            }
        }
    }
    sGlowOn = 0;
}

static void glow_update(void) {
    u32 table = Mc_ReadU32(REO_US_ROOM_DRAW + REO_ROOM_MODELS);
    u32 k, a, want, m;
    f32 off = Mc_ReadF32(REO_US_ROOM_DRAW + REO_ROOM_OFFSET);
    Mc_Vec3 eye;

    if (sGlowOn && (table != sGlowTable || !mirror_room())) {
        glow_restore();
    }
    mc_read_camera_eye(&eye);
    if (!mirror_room() || !mirror_in_ram(table, REO_MODEL_STRIDE * MC_MIR_OBJECTS) || !Mc_IsSaneF32(eye.x) ||
        !Mc_IsSaneF32(off)) {
        return;
    }
    if (!sGlowOn) {
        for (k = 0; k < MC_GLOW_CARDS; k++) {
            m = REO_US_OBJ_MATRIX + REO_OBJ_MATRIX_STRIDE * kGlowCard[k] + REO_OBJ_MATRIX_ORIGIN;
            if (!(Mc_ReadF32(m + 4) > 274.5f && Mc_ReadF32(m + 4) < 275.5f && Mc_ReadF32(m + 8) > 13033.0f &&
                  Mc_ReadF32(m + 8) < 13034.0f && Mc_IsSaneF32(Mc_ReadF32(m)))) {
                return; // (not the cards where the game puts them)
            }
        }
        for (k = 0; k < MC_GLOW_CARDS; k++) {
            sGlowOrig[k] = (u8)Mc_ReadU8(table + REO_MODEL_STRIDE * kGlowCard[k] + REO_MODEL_ZWRITE);
            sGlowMine[k] = sGlowOrig[k];
        }
        sGlowTable = table;
        sGlowOn = 1;
    }
    for (k = 0; k < MC_GLOW_CARDS; k++) {
        a = table + REO_MODEL_STRIDE * kGlowCard[k] + REO_MODEL_ZWRITE;
        m = REO_US_OBJ_MATRIX + REO_OBJ_MATRIX_STRIDE * kGlowCard[k] + REO_OBJ_MATRIX_ORIGIN;
        want = eye.x < Mc_ReadF32(m) + off ? 0u : sGlowOrig[k];
        if (Mc_ReadU8(a) != want) {
            Mc_WriteU8(a, want);
        }
        sGlowMine[k] = (u8)want;
    }
}

// ---- The objects' bounds --------------------------------------------------------------------------------------------
// A model's box from its packet's vertices (model space); 0 when the packet is not as expected or its vertices are not
// all found (the object is then always drawn).
static u32 model_box(u32 handle, f32* box) {
    u32 rec = Mc_ReadU32(REO_US_MODEL_RECORDS) + REO_MODEL_REC_STRIDE * (handle - 1);
    u32 d, end, a, n = 0, want, k;

    if (!mirror_in_ram(rec, REO_MODEL_REC_STRIDE)) {
        return 0;
    }
    d = Mc_ReadU32(rec + REO_MREC_PACKET);
    end = Mc_ReadU32(rec + REO_MREC_CHAINS_END);
    want = Mc_ReadU32(rec + REO_MREC_VERTICES);
    if ((d & 15u) || end == 0 || end > 0x100000u || !mirror_in_ram(d, end)) {
        return 0;
    }
    end += d;
    for (k = 0; k < 3; k++) {
        box[k] = 1e9f;
        box[3 + k] = -1e9f;
    }
    for (a = d; a < end;) {
        u32 tag = Mc_ReadU32(a);
        u32 b = a + 8;
        u32 stop = a + 16 + 16 * (tag & 0xFFFFu);

        if (((tag >> 28) & 7u) != 6u || stop > end) {
            return 0;
        }
        while (b < stop) {
            u32 w = Mc_ReadU32(b);
            u32 cmd = (w >> 24) & 0x7Fu;

            b += 4;
            if (cmd >= 0x60u) {
                u32 vn = (cmd >> 2) & 3u;
                u32 vl = cmd & 3u;
                u32 num = (w >> 16) & 0xFFu;
                u32 q, qend;

                num = num ? num : 256u;
                qend = b + ((32u >> vl) * (vn + 1u) * num + 31u) / 32u * 4u;
                if ((cmd & 0x6Fu) == 0x6Cu) { // V4-32: a GIF tag of the three registers, then its vertices
                    for (q = b; q + 16u <= qend && q + 16u <= stop;) {
                        if ((Mc_ReadU32(q + 4) >> 28) == 3u && Mc_ReadU32(q + 8) == 0x412u) {
                            u32 loop = Mc_ReadU32(q) & 0x7FFFu;
                            u32 v;

                            q += 16;
                            for (v = 0; v < loop && q + 64u <= qend; v++, q += 64) {
                                f32 x = Mc_ReadF32(q);
                                f32 y = Mc_ReadF32(q + 4);
                                f32 z = Mc_ReadF32(q + 8);

                                box[0] = mc_minf(box[0], x);
                                box[1] = mc_minf(box[1], y);
                                box[2] = mc_minf(box[2], z);
                                box[3] = mc_maxf(box[3], x);
                                box[4] = mc_maxf(box[4], y);
                                box[5] = mc_maxf(box[5], z);
                            }
                            n += v;
                        } else {
                            q += 16;
                        }
                    }
                }
                b = qend;
            } else if (cmd == 0x20u) {
                b += 4; // (STMASK)
            } else if (cmd == 0x30u || cmd == 0x31u) {
                b += 16; // (STROW, STCOL)
            } else if (cmd > 0x17u || (cmd > 0x07u && cmd < 0x10u) || cmd == 0x12u || cmd == 0x16u) {
                return 0; // (DIRECT, MPG and the unknown: not in these models)
            }
        }
        a = stop;
    }
    // (a NaN or a far vertex makes the box unusable: the object is then always drawn)
    for (k = 0; k < 6; k++) {
        if (!Mc_IsSaneF32(box[k])) {
            return 0;
        }
    }
    return n != 0 && n == want;
}

// The next objects' boxes (at most MC_MIR_BOUNDS_STEP vertices a frame), once per room.
static void bounds_step(void) {
    u32 table = Mc_ReadU32(REO_US_ROOM_DRAW + REO_ROOM_MODELS);
    u32 used = 0;
    u32 i, handle, w;

    if (table != sBoxTable) {
        sBoxTable = table;
        sBoxNext = 0;
        for (w = 0; w < REO_CUT_MASK_WORDS; w++) {
            sBoxOk[w] = 0;
            sBoxInWorld[w] = 0;
        }
    }
    if (!mirror_in_ram(table, REO_MODEL_STRIDE * MC_MIR_OBJECTS)) {
        sBoxNext = MC_MIR_OBJECTS;
        return;
    }
    while (sBoxNext < MC_MIR_OBJECTS && used < MC_MIR_BOUNDS_STEP) {
        i = sBoxNext++;
        handle = Mc_ReadU32(table + REO_MODEL_STRIDE * i);
        if (handle == 0 || handle == 0xFFFFFFFFu || handle > 0x10000u) {
            continue;
        }
        if (model_box(handle, sBox[i])) {
            sBoxOk[i >> 5] |= 1u << (i & 31);
        }
        used += Mc_ReadU32(Mc_ReadU32(REO_US_MODEL_RECORDS) + REO_MODEL_REC_STRIDE * (handle - 1) + REO_MREC_VERTICES);
    }
}

static u32 mirror_call(u32 fn, u32 nargs, u32 a0, u32 a1, u32 a2, u32 a3, u32 t0) {
    u32 args[5];

    args[0] = a0;
    args[1] = a1;
    args[2] = a2;
    args[3] = a3;
    args[4] = t0;
    sMirCalls++;
    return reo_guest_call(fn, args, nargs);
}

// The part of a pane on the screen: its corners in the view (v' = p V, rows), the part in front of the near plane onto
// the frame, that outline cut to the frame. 1 when some of the pane is on the screen (its outline's box on the frame
// then in o: x0, y0, x1, y1).
static u32 mirror_pane(const f32* v, f32 x0, f32 x1, f32 kx, f32 ky, f32 w, f32 h, f32* o) {
    f32 c[4][3];
    f32 pa[2][20];
    u32 i, j, e, n = 0, cnt, src = 0;

    for (i = 0; i < 4; i++) {
        f32 x = (i == 1 || i == 2) ? x1 : x0;
        f32 y = i >= 2 ? MC_MIR_PANE_Y1 : MC_MIR_PANE_Y0;

        for (j = 0; j < 3; j++) {
            c[i][j] = x * v[j] + y * v[4 + j] + MC_MIR_PLANE * v[8 + j] + v[12 + j];
        }
    }
    // In front of the near plane, onto the frame (a convex outline of at most 5 corners; each cut adds at most one).
    for (i = 0; i < 4; i++) {
        const f32* a = c[i];
        const f32* b = c[(i + 1) & 3];
        u32 inA = a[2] <= -MC_MIR_NEAR;
        u32 inB = b[2] <= -MC_MIR_NEAR;
        f32 q[3];

        for (j = 0; j < 2; j++) {
            if (j == 0) {
                if (!inA) {
                    continue;
                }
                q[0] = a[0];
                q[1] = a[1];
                q[2] = a[2];
            } else {
                f32 t;

                if (inA == inB) {
                    continue;
                }
                t = (-MC_MIR_NEAR - a[2]) / (b[2] - a[2]);
                q[0] = a[0] + (b[0] - a[0]) * t;
                q[1] = a[1] + (b[1] - a[1]) * t;
                q[2] = -MC_MIR_NEAR;
            }
            pa[0][2 * n] = w * 0.5f * (1.0f + kx * q[0] / -q[2]);
            pa[0][2 * n + 1] = h * 0.5f * (1.0f - ky * q[1] / -q[2]);
            if (!Mc_IsSaneF32(pa[0][2 * n]) || !Mc_IsSaneF32(pa[0][2 * n + 1])) {
                return 0;
            }
            n++;
        }
    }
    // Cut to the frame: x >= 0, y >= 0, x <= w, y <= h.
    for (e = 0; e < 4 && n >= 3; e++) {
        const f32* in = pa[src];
        f32* out = pa[src ^ 1u];
        u32 axis = e & 1u;
        f32 lim = e < 2 ? 0.0f : (axis ? h : w);
        f32 sgn = e < 2 ? 1.0f : -1.0f;

        cnt = 0;
        for (i = 0; i < n; i++) {
            const f32* p0 = in + 2 * i;
            const f32* p1 = in + 2 * ((i + 1) % n);
            f32 d0 = sgn * (p0[axis] - lim);
            f32 d1 = sgn * (p1[axis] - lim);

            if (d0 >= 0.0f && cnt < 10) {
                out[2 * cnt] = p0[0];
                out[2 * cnt + 1] = p0[1];
                cnt++;
            }
            if ((d0 >= 0.0f) != (d1 >= 0.0f) && cnt < 10) {
                f32 t = d0 / (d0 - d1);

                out[2 * cnt] = p0[0] + (p1[0] - p0[0]) * t;
                out[2 * cnt + 1] = p0[1] + (p1[1] - p0[1]) * t;
                cnt++;
            }
        }
        n = cnt;
        src ^= 1u;
    }
    if (n < 3) {
        return 0;
    }
    o[0] = o[2] = pa[src][0];
    o[1] = o[3] = pa[src][1];
    for (i = 1; i < n; i++) {
        o[0] = mc_minf(o[0], pa[src][2 * i]);
        o[1] = mc_minf(o[1], pa[src][2 * i + 1]);
        o[2] = mc_maxf(o[2], pa[src][2 * i]);
        o[3] = mc_maxf(o[3], pa[src][2 * i + 1]);
    }
    return 1;
}

// The part of pane k on the screen, on the plane: the rays from the eye through the corners of its outline's box on
// the frame (+ the margin) met with the plane, cut to the pane. The whole pane when a ray does not meet the plane.
static void mirror_pane_part(u32 k, const f32* v, const Mc_Vec3* eye, const f32* o, f32 kx, f32 ky, f32 w, f32 h) {
    f32 x0 = kMirPaneX[2 * k];
    f32 x1 = kMirPaneX[2 * k + 1];
    f32 lx = 1e9f, ly = 1e9f, hx = -1e9f, hy = -1e9f;
    u32 i;

    for (i = 0; i < 4; i++) {
        f32 sx = (i & 1u) ? o[2] + MC_MIR_MARGIN : o[0] - MC_MIR_MARGIN;
        f32 sy = (i & 2u) ? o[3] + MC_MIR_MARGIN : o[1] - MC_MIR_MARGIN;
        f32 vx = (2.0f * sx / w - 1.0f) / kx;
        f32 vy = (1.0f - 2.0f * sy / h) / ky;
        f32 dx = vx * v[0] + vy * v[1] - v[2];
        f32 dy = vx * v[4] + vy * v[5] - v[6];
        f32 dz = vx * v[8] + vy * v[9] - v[10];
        f32 t;

        if (!(dz < -1e-4f)) {
            lx = x0;
            ly = MC_MIR_PANE_Y0;
            hx = x1;
            hy = MC_MIR_PANE_Y1;
            break;
        }
        t = (MC_MIR_PLANE - eye->z) / dz;
        lx = mc_minf(lx, eye->x + dx * t);
        hx = mc_maxf(hx, eye->x + dx * t);
        ly = mc_minf(ly, eye->y + dy * t);
        hy = mc_maxf(hy, eye->y + dy * t);
    }
    sMirPart[k][0] = mc_maxf(lx, x0);
    sMirPart[k][1] = mc_maxf(ly, MC_MIR_PANE_Y0);
    sMirPart[k][2] = mc_minf(hx, x1);
    sMirPart[k][3] = mc_minf(hy, MC_MIR_PANE_Y1);
    sMirSeen[k] = sMirPart[k][0] <= sMirPart[k][2] && sMirPart[k][1] <= sMirPart[k][3];
}

// Where object i's world matrix is (the room's own, or the one its pointer gives).
static u32 mirror_matrix(u32 i) {
    u32 src = REO_US_ROOM_DRAW + REO_ROOM_MATRIX + REO_OBJ_MATRIX_STRIDE * i;

    if ((Mc_ReadU32(REO_US_ROOM_DRAW + REO_ROOM_MAT_SEL + 4 * (i >> 5)) >> (i & 31)) & 1) {
        u32 at = Mc_ReadU32(REO_US_ROOM_DRAW + REO_ROOM_MAT_PTR + 4 * i);

        if (at != 0) {
            src = at;
        }
    }
    return src;
}

// Object i's box in the world: its centre and half extents through the matrix's rows (at src), its origin o.
static u32 mirror_world_box(u32 i, u32 src, const f32* o) {
    const f32* b = sBox[i];
    f32* w = sBoxWorld[i];
    f32 c[3], e[3];
    u32 j, k;

    for (j = 0; j < 3; j++) {
        c[j] = o[j];
        e[j] = 0.0f;
    }
    for (k = 0; k < 3; k++) {
        f32 bc = 0.5f * (b[k] + b[3 + k]);
        f32 be = 0.5f * (b[3 + k] - b[k]);

        for (j = 0; j < 3; j++) {
            f32 a = Mc_ReadF32(src + 16 * k + 4 * j);

            c[j] += bc * a;
            e[j] += be * mc_absf(a);
        }
    }
    for (j = 0; j < 3; j++) {
        w[j] = c[j] - e[j];
        w[3 + j] = c[j] + e[j];
        if (!Mc_IsSaneF32(w[j]) || !Mc_IsSaneF32(w[3 + j])) {
            return 0;
        }
        sBoxAt[i][j] = o[j];
    }
    sBoxInWorld[i >> 5] |= 1u << (i & 31);
    return 1;
}

// The frame's plan for the group's objects: the ones the frame draws in front of the mirror wall (not the counter
// top), each with the panes whose part on the screen the place of its reflection on the plane meets: the part of its
// world box in front of the plane, mirrored, seen from the eye (the rays of its corners met with the plane).
static void mirror_plan(u32 first, u32 count, const Mc_Vec3* eye) {
    u32 table = Mc_ReadU32(REO_US_ROOM_DRAW + REO_ROOM_MODELS);
    u32 i, j, k, f, src, bit;
    f32 off[3], o[3];

    for (j = 0; j < 3; j++) {
        off[j] = Mc_ReadF32(REO_US_ROOM_DRAW + REO_ROOM_OFFSET + 4 * j);
    }
    for (i = first; i < first + count && i < MC_MIR_OBJECTS; i++) {
        const f32* w = sBoxWorld[i];
        f32 zn, tf, tn, r[4];

        sPlanPanes[i] = 0;
        f = Mc_ReadU8(REO_US_OBJ_FLAGS + i);
        if (!(f & 1) || (f & 0x80) || i == MC_MIR_COUNTER) {
            continue;
        }
        src = mirror_matrix(i);
        for (j = 0; j < 3; j++) {
            o[j] = Mc_ReadF32(src + 0x30 + 4 * j) + off[j];
        }
        if (!(o[2] >= MC_MIR_PLANE + MC_MIR_WALL_DEPTH) || !Mc_IsSaneF32(o[0]) || !Mc_IsSaneF32(o[1]) ||
            !Mc_IsSaneF32(o[2])) {
            continue; // (behind the glass - the copy - or on the mirror wall)
        }
        sPlanPanes[i] = 3;
        bit = 1u << (i & 31);
        if (table != sBoxTable || !(sBoxOk[i >> 5] & bit)) {
            continue; // (no box yet: drawn)
        }
        if ((!(sBoxInWorld[i >> 5] & bit) || o[0] != sBoxAt[i][0] || o[1] != sBoxAt[i][1] || o[2] != sBoxAt[i][2]) &&
            !mirror_world_box(i, src, o)) {
            continue;
        }
        sPlanPanes[i] = 0;
        if (w[5] <= MC_MIR_PLANE) {
            continue; // (all of it behind the plane)
        }
        // Mirrored: its far side at 2 plane - z1, its near side at 2 plane - max(z0, plane); t of the eye's ray there.
        zn = mc_maxf(w[2], MC_MIR_PLANE);
        tf = (MC_MIR_PLANE - eye->z) / (2.0f * MC_MIR_PLANE - w[5] - eye->z);
        tn = (MC_MIR_PLANE - eye->z) / (2.0f * MC_MIR_PLANE - zn - eye->z);
        r[0] = mc_minf(eye->x + tf * (w[0] - eye->x), eye->x + tn * (w[0] - eye->x));
        r[1] = mc_minf(eye->y + tf * (w[1] - eye->y), eye->y + tn * (w[1] - eye->y));
        r[2] = mc_maxf(eye->x + tf * (w[3] - eye->x), eye->x + tn * (w[3] - eye->x));
        r[3] = mc_maxf(eye->y + tf * (w[4] - eye->y), eye->y + tn * (w[4] - eye->y));
        for (k = 0; k < 2; k++) {
            const f32* p = sMirPart[k];

            if (sMirSeen[k] && r[2] >= p[0] - MC_MIR_CULL_MARGIN && r[0] <= p[2] + MC_MIR_CULL_MARGIN &&
                r[3] >= p[1] - MC_MIR_CULL_MARGIN && r[1] <= p[3] + MC_MIR_CULL_MARGIN) {
                sPlanPanes[i] |= (u8)(1u << k);
            }
        }
    }
}

// Object i drawn mirrored about the glass's plane, as the game's room loop draws it.
static void mirror_object(u32 i) {
    u32 entry = Mc_ReadU32(REO_US_ROOM_DRAW + REO_ROOM_MODELS) + REO_MODEL_STRIDE * i;
    u32 handle = Mc_ReadU32(entry);
    u32 src = mirror_matrix(i);
    u32 m[16];
    u32 k, n, scratch;
    Mc_Word word;

    if (handle == 0 || handle == 0xFFFFFFFFu) {
        return;
    }
    for (k = 0; k < 16; k++) {
        m[k] = Mc_ReadU32(src + 4 * k);
    }
    // The origin + the room's offset, then mirrored in z: the axes' z negated, the origin's z to 2 plane - z.
    for (k = 0; k < 3; k++) {
        word.f = Mc_ReadF32(src + 0x30 + 4 * k) + Mc_ReadF32(REO_US_ROOM_DRAW + REO_ROOM_OFFSET + 4 * k);
        if (k == 2) {
            word.f = 2.0f * MC_MIR_PLANE - word.f;
        }
        m[12 + k] = word.u;
    }
    m[2] ^= 0x80000000u;
    m[6] ^= 0x80000000u;
    m[10] ^= 0x80000000u;
    scratch = reo_guest_scratch(m, 16);
    if (scratch == 0) {
        return;
    }
    mirror_call(REO_US_FN_SET_SLOT, 2, REO_SLOT_WORLD, scratch, 0, 0, 0);
    if (Mc_ReadU8(entry + REO_MODEL_HAS_RSTATES)) {
        mirror_call(REO_US_FN_RSTATES, 2, entry, REO_US_ROOM_DRAW + REO_ROOM_RSTATES + 4 * i, 0, 0, 0);
    }
    n = Mc_ReadU32(entry + REO_MODEL_COUNT);
    if (n > 32) {
        n = 32;
    }
    for (k = 0; k < n; k++) {
        u32 record = Mc_ReadU16(REO_US_ROOM_DRAW + REO_ROOM_MATERIAL + 2 * Mc_ReadU32(entry + REO_MODEL_INDEX + 4 * k));
        u32 mat = REO_US_MATERIALS + REO_MATERIAL_STRIDE * record;

        if (sSlotMat[k] != mat) {
            mirror_call(REO_US_FN_SET_SLOT, 2, REO_SLOT_MATERIAL + k, mat, 0, 0, 0);
            sSlotMat[k] = mat;
        }
    }
    mirror_call(REO_US_FN_DRAW_MODEL, 2, handle, 0, 0, 0, 0);
}

// A box on the frame (x0, y0, x1, y1) + the margin, cut to the frame: the scissor rectangle r (x0, y0, x1, y1); its
// area (0: none).
static u32 mirror_rect(const f32* o, u32 w, u32 h, u32* r) {
    r[0] = o[0] - MC_MIR_MARGIN <= 0.0f ? 0 : (u32)(o[0] - MC_MIR_MARGIN);
    r[1] = o[1] - MC_MIR_MARGIN <= 0.0f ? 0 : (u32)(o[1] - MC_MIR_MARGIN);
    r[2] = o[2] + MC_MIR_MARGIN >= (f32)(w - 1) ? w - 1 : (u32)(o[2] + MC_MIR_MARGIN);
    r[3] = o[3] + MC_MIR_MARGIN >= (f32)(h - 1) ? h - 1 : (u32)(o[3] + MC_MIR_MARGIN);
    return r[2] < r[0] || r[3] < r[1] ? 0 : (r[2] - r[0] + 1) * (r[3] - r[1] + 1);
}

// One pass of the reflection: the scissor rectangle r, the objects the panes of `panes` can show.
static void mirror_pass(u32 first, u32 count, const u32* r, u32 panes) {
    u32 i;

    if (mirror_call(REO_US_FN_SCISSOR, 5, r[0], r[1], r[2] - r[0] + 1, r[3] - r[1] + 1, 0) != 1) {
        return; // (refused: this camera does not drive the camera)
    }
    for (i = first; i < first + count && i < MC_MIR_OBJECTS && sMirCalls < MC_MIR_MAX_CALLS; i++) {
        if (sPlanPanes[i] & panes) {
            mirror_object(i);
        }
    }
}

void mc_mirror_room_draw(u32 room, u32 group) {
    u32 first, count, k, w, h, both;
    u32 area[2], r[3][4], old[4];
    f32 v[16], o[3][4];
    f32 fov, aspect, half, kx, ky;
    Mc_Vec3 eye;

    sMirMarked = 1;
    // Only while the camera drives with the artwork kept (the copy dropped from every cut), in the restroom.
    if (room != REO_US_ROOM_DRAW || !sArtOn || !((sArtDrop[MC_MIR_COPY >> 5] >> (MC_MIR_COPY & 31)) & 1) ||
        !mirror_room() || group >= Mc_ReadU8(REO_US_ROOM_DRAW + REO_ROOM_GROUPS)) {
        return;
    }
    // The panes on the screen: the frame's view and its projection (vertical field of view, aspect ratio).
    for (k = 0; k < 16; k++) {
        v[k] = Mc_ReadF32(REO_US_VIEW_MATRIX + 4 * k);
        if (!Mc_IsSaneF32(v[k])) {
            return;
        }
    }
    // The frame's eye (v' = p V: p = -t R^T).
    eye.x = -(v[12] * v[0] + v[13] * v[1] + v[14] * v[2]);
    eye.y = -(v[12] * v[4] + v[13] * v[5] + v[14] * v[6]);
    eye.z = -(v[12] * v[8] + v[13] * v[9] + v[14] * v[10]);
    if (!Mc_IsSaneF32(eye.x) || !Mc_IsSaneF32(eye.y) || !Mc_IsSaneF32(eye.z) || !(eye.z > MC_MIR_PLANE + 1.0f)) {
        return;
    }
    fov = Mc_ReadF32(REO_US_CAM_FOV_DEG);
    aspect = Mc_ReadF32(REO_US_CAM_ASPECT);
    w = Mc_ReadU32(REO_US_SCREEN_W);
    h = Mc_ReadU32(REO_US_SCREEN_H);
    if (!Mc_IsSaneF32(fov) || fov < 1.0f || fov > 170.0f || !Mc_IsSaneF32(aspect) || aspect < 0.5f || aspect > 5.0f ||
        w < 64 || w > 2048 || h < 64 || h > 2048) {
        return;
    }
    half = fov * 0.5f * MC_DEG2RAD;
    ky = mc_cosf(half) / mc_sinf(half);
    kx = ky / aspect;
    for (k = 0; k < 2; k++) {
        sMirSeen[k] = mirror_pane(v, kMirPaneX[2 * k], kMirPaneX[2 * k + 1], kx, ky, (f32)w, (f32)h, o[k]);
        area[k] = sMirSeen[k] ? mirror_rect(o[k], w, h, r[k]) : 0;
        if (area[k]) {
            mirror_pane_part(k, v, &eye, o[k], kx, ky, (f32)w, (f32)h);
        }
        sMirSeen[k] = area[k] && sMirSeen[k];
    }
    if (!sMirSeen[0] && !sMirSeen[1]) {
        return; // (the glass is not on the screen)
    }
    both = 0;
    if (sMirSeen[0] && sMirSeen[1]) {
        o[2][0] = mc_minf(o[0][0], o[1][0]);
        o[2][1] = mc_minf(o[0][1], o[1][1]);
        o[2][2] = mc_maxf(o[0][2], o[1][2]);
        o[2][3] = mc_maxf(o[0][3], o[1][3]);
        // (two rectangles that meet would draw what both panes show twice there: one pass then)
        both = mirror_rect(o[2], w, h, r[2]) <= area[0] + area[1] ||
               (r[0][0] <= r[1][2] && r[1][0] <= r[0][2] && r[0][1] <= r[1][3] && r[1][1] <= r[0][3]);
    }
    for (k = 0; k < 4; k++) {
        old[k] = Mc_ReadU32(REO_US_SCISSOR_1 + 4 * k);
    }
    if (old[2] < old[0] || old[3] < old[1] || old[2] >= 2048 || old[3] >= 2048) {
        return;
    }
    // The copy is never drawn with the reflection (Show Hidden Objects sets every object's flag at every frame).
    for (k = 0; k < 2; k++) {
        u32 f = Mc_ReadU8(REO_US_OBJ_FLAGS + (k ? MC_MIR_POSTERS : MC_MIR_COPY));

        if (f & 1) {
            Mc_WriteU8(REO_US_OBJ_FLAGS + (k ? MC_MIR_POSTERS : MC_MIR_COPY), f & ~1u);
        }
    }
    first = Mc_ReadU8(REO_US_ROOM_DRAW + REO_ROOM_GROUP_FIRST + group);
    count = Mc_ReadU8(REO_US_ROOM_DRAW + REO_ROOM_GROUP_COUNT + group);
    mirror_plan(first, count, &eye);
    sMirCalls = 0;
    for (k = 0; k < 32; k++) {
        sSlotMat[k] = 0; // (the room loop sets its own; this draw's are set at least once)
    }
    if (both) {
        mirror_pass(first, count, r[2], 3u);
    } else {
        for (k = 0; k < 2; k++) {
            if (sMirSeen[k]) {
                mirror_pass(first, count, r[k], 1u << k);
            }
        }
    }
    mirror_call(REO_US_FN_SCISSOR, 5, old[0], old[1], old[2] - old[0] + 1, old[3] - old[1] + 1, 0);
}

// While the camera drives with the artwork kept: the glow cards as the fixed camera shows them, and the objects'
// bounds read in the restroom (the reflection's plan).
static void mirror_frame(void) {
    if (!sArtOn) {
        glow_restore();
        return;
    }
    glow_update();
    if (sMirMarked && mirror_room()) {
        bounds_step();
    }
}

static u32 sHaveCull = 0;
static f32 sCullOrig[REO_US_CULL_COUNT];

void mc_set_near_hide_scale(f32 scale) {
    u32 i;

    // Never above the game's radii: a larger radius only hides more of what is near the camera.
    scale = mc_clampf(scale, 0.0f, 1.0f);
    if (!sHaveCull) {
        for (i = 0; i < REO_US_CULL_COUNT; i++) {
            sCullOrig[i] = Mc_ReadF32(REO_US_CULL_TABLE + i * 4);
        }
        sHaveCull = 1;
    }
    // Always scaled from the originals, never compounded.
    for (i = 0; i < REO_US_CULL_COUNT; i++) {
        Mc_WriteF32(REO_US_CULL_TABLE + i * 4, sCullOrig[i] * scale);
    }
}

void mc_restore_near_hide(void) {
    u32 i;

    if (!sHaveCull) {
        return;
    }
    for (i = 0; i < REO_US_CULL_COUNT; i++) {
        Mc_WriteF32(REO_US_CULL_TABLE + i * 4, sCullOrig[i]);
    }
    sHaveCull = 0;
}

static u32 sHaveLight = 0;
static u32 sLightOrig;

void mc_set_entity_light(u32 rgb) {
    // The exe's set_entity_light((r, g, b)): colour bytes R, G, B, alpha 0xFF, then the mode byte 2.
    u32 r = (rgb >> 16) & 0xFF;
    u32 g = (rgb >> 8) & 0xFF;
    u32 b = rgb & 0xFF;
    u32 rgba = r | (g << 8) | (b << 16) | (0xFFu << 24);

    if (!sHaveLight) {
        sLightOrig = Mc_ReadU32(REO_US_ENTITY_LIGHT);
        sHaveLight = 1;
    }
    if ((Mc_ReadU32(REO_US_ENTITY_LIGHT) & 0x00FFFFFFu) != (rgba & 0x00FFFFFFu)) {
        // Colour first, then the mode byte.
        Mc_WriteU32(REO_US_ENTITY_LIGHT, rgba);
        Mc_WriteU8(REO_US_ENTITY_LIGHT_MODE, 2);
    }
}

void mc_restore_entity_light(void) {
    if (sHaveLight) {
        Mc_WriteU32(REO_US_ENTITY_LIGHT, sLightOrig);
        Mc_WriteU8(REO_US_ENTITY_LIGHT_MODE, 2);
        sHaveLight = 0;
    }
}

// ---- Pad -------------------------------------------------------------------------------------------------------

// ModernCam's stick deadzone: |v| < 7000 of 32767 reads as 0, with no rescale.
#define MC_STICK_DEADZONE (7000.0f / 32767.0f)

static u32 pad_block(void) {
    u32 blk = Mc_ReadU32(REO_US_PAD_PTR);

    if (blk < 0x100000 || blk + 2 * REO_US_PADFRAME_SIZE > 0x2000000 || (blk & 3)) {
        return 0;
    }
    return blk;
}

static f32 stick_axis(u32 raw, u32 flip) {
    f32 v = ((f32)raw - 128.0f) / 127.0f;

    v = mc_clampf(v, -1.0f, 1.0f);
    if (mc_absf(v) < MC_STICK_DEADZONE) {
        v = 0.0f;
    }
    return flip ? -v : v;
}

void mc_pad_read(Mc_Pad* pad) {
    u32 blk = pad_block();
    u32 frame;
    u32 len;

    pad->valid = 0;
    pad->held = 0;
    pad->lx = pad->ly = pad->rx = pad->ry = 0.0f;
    if (!blk) {
        return;
    }
    // The newer of the two frames, as the game's own frame getter picks it.
    frame = blk;
    if ((s32)Mc_ReadU32(blk + REO_US_PADFRAME_COUNT) <
        (s32)Mc_ReadU32(blk + REO_US_PADFRAME_SIZE + REO_US_PADFRAME_COUNT)) {
        frame = blk + REO_US_PADFRAME_SIZE;
    }
    len = Mc_ReadU8(frame + REO_US_PADFRAME_LEN);
    if (len < 2) {
        return;
    }
    pad->valid = 1;
    pad->held = ~Mc_ReadU16(frame + REO_US_PADFRAME_BTN) & 0xFFFF;
    if (len >= 6) {
        pad->rx = stick_axis(Mc_ReadU8(frame + REO_US_PADFRAME_RX), 0);
        pad->ry = stick_axis(Mc_ReadU8(frame + REO_US_PADFRAME_RY), 1); // up = +
        pad->lx = stick_axis(Mc_ReadU8(frame + REO_US_PADFRAME_LX), 0);
        pad->ly = stick_axis(Mc_ReadU8(frame + REO_US_PADFRAME_LY), 1);
    }
}

static void centre_group(u32 addr) {
    // A centred stick as the mapper sees one: magnitude 0, angle 0xC000, x 0, y 0.
    Mc_WriteU32(addr, 0xC0000000u);
    Mc_WriteU32(addr + 4, 0);
}

static u32 sce_to_src(u32 sce) {
    u32 out = 0;

    if (sce & REO_SCE_SQUARE) out |= REO_SRC_SQUARE;
    if (sce & REO_SCE_CROSS) out |= REO_SRC_CROSS;
    if (sce & REO_SCE_CIRCLE) out |= REO_SRC_CIRCLE;
    if (sce & REO_SCE_R1) out |= REO_SRC_R1;
    if (sce & REO_SCE_L1) out |= REO_SRC_L1;
    if (sce & REO_SCE_R2) out |= REO_SRC_R2;
    if (sce & REO_SCE_L2) out |= REO_SRC_L2;
    return out;
}

u32 mc_padmap_player_word(void) {
    // Offline the local player is mapped record 0 (the mapper's index for her, 0x001B0C08..0x001B0C30).
    return REO_US_PAD_REC + REO_PADREC_HELD;
}

void mc_padmap_apply(u32 heldWordAddr, u32 pressSce, u32 suppressSce, u32 centreLeft, u32 centreRight) {
    u32 press = sce_to_src(pressSce);
    u32 suppress = sce_to_src(suppressSce);
    u32 rec = heldWordAddr - REO_PADREC_HELD;

    if (press || suppress) {
        u32 cur = Mc_ReadU32(heldWordAddr);
        u32 next = (cur & ~suppress) | press; // press beats suppress

        if (next != cur) {
            Mc_WriteU32(heldWordAddr, next);
        }
    }
    if (centreLeft) {
        centre_group(rec + REO_PADREC_LSTICK);
    }
    if (centreRight) {
        centre_group(rec + REO_PADREC_RSTICK);
    }
}

u32 mc_button_for_action(u32 actionBit) {
    // [square, cross, circle] action words for the player's control type, as the mapper reads them.
    static const u16 sSce[3] = { REO_SCE_SQUARE, REO_SCE_CROSS, REO_SCE_CIRCLE };
    u32 type = Mc_ReadU8(REO_US_CONTROL_TYPE) & 3;
    u32 i;

    for (i = 0; i < 3; i++) {
        if (Mc_ReadU32(REO_US_BTN_MAP_TABLE + type * 12 + i * 4) & actionBit) {
            return sSce[i];
        }
    }
    return 0;
}

// ---- Items -----------------------------------------------------------------------------------------------------

static u32 item_addr(u32 slot) {
    return REO_US_ITEM_TABLE + slot * REO_ITEM_STRIDE;
}

static u32 def_addr(u32 type) {
    return REO_US_ITEM_DEFS + type * REO_DEF_STRIDE;
}

// The weapon's combine rules accept this item (0x005B3E40: the other item's id in the weapon type's rule list).
// Returns 2 when the rule list cannot be read (then every carried item is a candidate, as in the exe).
static u32 rules_accept(u32 weaponType, u32 itemId) {
    u32 d = def_addr(weaponType);
    s32 off = Mc_ReadS16(d + REO_DEF_RULE_OFF);
    s32 n = Mc_ReadS16(d + REO_DEF_RULE_N);
    s32 i;

    if (off < 0 || n <= 0 || n > 64) {
        return 2;
    }
    for (i = 0; i < n; i++) {
        u32 e = REO_US_ITEM_RULES + (u32)off * 2 + (u32)i * REO_RULE_STRIDE;

        if ((u32)Mc_ReadU16(e) == itemId) {
            return 1;
        }
    }
    return 0;
}

u32 mc_find_reload(Mc_ReloadPlan* plan) {
    u32 owner = mc_player_slot() + 1;
    u32 gun = Mc_ReadU8(mc_char_base(mc_player_slot()) + REO_CHAR_EQUIPPED);
    u32 ga, gtype, cap, count, bestSlot = 0, bestRank = 99, n = 0, slot;

    if (gun == 0 || gun >= REO_ITEM_SLOTS) {
        return 0;
    }
    ga = item_addr(gun);
    if (Mc_ReadU16(ga + REO_ITEM_OWNER) != owner) {
        return 0; // not carried
    }
    gtype = Mc_ReadU8(ga + REO_ITEM_TYPE);
    count = Mc_ReadU32(ga + REO_ITEM_COUNT);
    cap = (u32)Mc_ReadS16(def_addr(gtype) + REO_DEF_CAP);
    if ((s32)cap <= 1 || (s32)(cap - count) <= 0) {
        return 0; // magazine full, or not a weapon with a magazine
    }
    for (slot = 0; slot < REO_ITEM_SLOTS; slot++) {
        u32 a = item_addr(slot);
        u32 itemCount, itemType, accept, rank;

        if (slot == gun || Mc_ReadU16(a + REO_ITEM_OWNER) != owner) {
            continue;
        }
        itemCount = Mc_ReadU32(a + REO_ITEM_COUNT);
        itemType = Mc_ReadU8(a + REO_ITEM_TYPE);
        if (itemCount == 0) {
            continue;
        }
        accept = rules_accept(gtype, Mc_ReadU16(a + REO_ITEM_ID));
        if (accept == 0) {
            continue;
        }
        n++;
        // ModernCam's sort: loose rounds (definition flag 2) first, then the rest in slot order.
        rank = (Mc_ReadU8(def_addr(itemType) + REO_DEF_FLAGS) & REO_DEF_FLAG_ROUNDS) ? 0 : 1;
        if (rank < bestRank) {
            bestRank = rank;
            bestSlot = slot;
        }
    }
    if (n == 0) {
        return 0; // no matching ammo carried
    }
    plan->weaponSlot = gun;
    plan->ammoSlot = bestSlot;
    plan->count = count;
    plan->cap = cap;
    plan->candidates = n;
    return 1;
}
