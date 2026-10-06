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
