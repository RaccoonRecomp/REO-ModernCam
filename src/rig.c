#include "mcam_rig.h"

// Ported from Outbreak ModernCam (ro_camera.py). Constants and formulas are unchanged; comments
// note where the original's behaviour is easy to get wrong.

#define MC_REF_DT (1.0f / 60.0f)

// Follow-heading tuning (FollowHeading class constants).
#define MC_FOLLOW_TELEPORT      800.0f // a single step longer than this is a warp or room change
#define MC_FOLLOW_MIN_STEP      0.3f   // smaller steps are the same position read back
#define MC_FOLLOW_BACKPEDAL_ON  135.0f
#define MC_FOLLOW_BACKPEDAL_OFF 115.0f

f32 mc_smooth_alpha(f32 k, f32 dt) {
    if (k <= 0.0f || dt <= 0.0f) {
        return 0.0f;
    }
    if (k >= 1.0f) {
        return 1.0f;
    }
    return 1.0f - mc_powf(1.0f - k, dt / MC_REF_DT);
}

f32 mc_decay_look(f32 off, f32 dt, f32 tau) {
    if (tau <= 0.0f || dt <= 0.0f) {
        return off;
    }
    return off * mc_expf(-dt / tau);
}

void mc_release_free_look(f32* camYaw, f32* lookOff, f32 dt, f32 tau) {
    // The camera and the offset move together: lookOff is defined as the camera's angle from
    // directly behind her, so winding it down without turning the camera would make it a lie.
    f32 newOff = mc_decay_look(*lookOff, dt, tau);

    *camYaw += newOff - *lookOff;
    *lookOff = newOff;
}

void mc_eye_and_look(f32 px, f32 py, f32 pz, f32 yawDeg, f32 pitchDeg, const Mc_Rig* rig, Mc_Vec3* eye, Mc_Vec3* look) {
    f32 tr = yawDeg * MC_DEG2RAD;
    f32 pr = pitchDeg * MC_DEG2RAD;
    f32 fx = mc_sinf(tr); // f = from her toward the eye ("behind")
    f32 fz = mc_cosf(tr);
    f32 rx = fz;          // r = screen-right
    f32 rz = -fx;
    f32 horiz = rig->distance * mc_cosf(pr);

    eye->x = px + fx * horiz + rx * rig->shoulder;
    eye->y = py + rig->height + rig->distance * mc_sinf(pr);
    eye->z = pz + fz * horiz + rz * rig->shoulder;
    look->x = px + rx * rig->shoulder;
    look->y = py + rig->height + rig->lookDrop;
    look->z = pz + rz * rig->shoulder;
}

f32 mc_view_elevation_deg(f32 pitchDeg, f32 distance, f32 lookDrop) {
    f32 pr = pitchDeg * MC_DEG2RAD;

    return mc_atan2f(lookDrop - distance * mc_sinf(pr), distance * mc_cosf(pr)) * MC_RAD2DEG;
}

u32 mc_compute_modern_movement(f32 lx, f32 ly, f32 viewYaw, f32 playerYaw, f32 deadzone, f32* desiredHeading, f32* gap) {
    f32 stickAngle;

    if (mc_hypotf(lx, ly) < deadzone) {
        *desiredHeading = 0.0f;
        *gap = 0.0f;
        return 0;
    }
    // Note the minus on lx: screen-right is viewYaw - 90 in this engine's heading convention.
    stickAngle = mc_atan2f(-lx, ly) * MC_RAD2DEG;
    *desiredHeading = mc_angwrap(viewYaw + stickAngle);
    *gap = mc_angwrap(*desiredHeading - playerYaw);
    return 1;
}

void mc_blend_rig(const Mc_Rig* hip, const Mc_Rig* aim, f32 t, Mc_Rig* out) {
    out->distance = mc_lerp(hip->distance, aim->distance, t);
    out->height = mc_lerp(hip->height, aim->height, t);
    out->shoulder = mc_lerp(hip->shoulder, aim->shoulder, t);
    out->lookDrop = mc_lerp(hip->lookDrop, aim->lookDrop, t);
    out->pitchTrim = mc_lerp(hip->pitchTrim, aim->pitchTrim, t);
    out->fovDeg = mc_lerp(hip->fovDeg, aim->fovDeg, t);
}

void mc_follow_reset(Mc_FollowHeading* f) {
    f->havePos = 0;
    f->since = 1e9f; // no step for ages: not moving
    f->haveStepYaw = 0;
    f->haveYaw = 0;
    f->backpedal = 0;
}

void mc_follow_init(Mc_FollowHeading* f) {
    f->moveHold = 0.18f;
    f->fast = 0.6f;
    f->slow = 0.15f;
    mc_follow_reset(f);
}

u32 mc_follow_update(Mc_FollowHeading* f, f32 px, f32 pz, f32 playerYaw, f32 dt, u32 inputMoving, u32 aiming, f32* baseYaw) {
    u32 moving;
    f32 stepYaw;
    f32 gap;

    if (!f->havePos) {
        f->havePos = 1;
        f->posX = px;
        f->posZ = pz;
    } else {
        f32 dx = px - f->posX;
        f32 dz = pz - f->posZ;
        f32 step = mc_hypotf(dx, dz);

        if (step > MC_FOLLOW_TELEPORT) {
            mc_follow_reset(f);
            f->havePos = 1;
            f->posX = px;
            f->posZ = pz;
        } else if (step >= MC_FOLLOW_MIN_STEP) {
            // Difference against the last position that actually changed, never per poll.
            f32 span = mc_minf(1.0f, f->since + dt);
            f32 raw = mc_atan2f(dx, dz) * MC_RAD2DEG;

            if (!f->haveYaw) {
                f->haveYaw = 1;
                f->fastYaw = raw;
                f->moveYaw = raw;
            } else {
                f->fastYaw += mc_angwrap(raw - f->fastYaw) * mc_smooth_alpha(f->fast, span);
                f->moveYaw += mc_angwrap(raw - f->moveYaw) * mc_smooth_alpha(f->slow, span);
            }
            f->posX = px;
            f->posZ = pz;
            f->since = 0.0f;
            f->haveStepYaw = 1;
            f->stepYaw = playerYaw; // her facing at the moment of this step
        } else {
            f->since += dt;
        }
    }

    moving = f->since <= f->moveHold;

    if (aiming || !f->haveYaw || (!moving && inputMoving)) {
        f->haveYaw = 1;
        f->fastYaw = playerYaw;
        f->moveYaw = playerYaw;
        f->haveStepYaw = 1;
        f->stepYaw = playerYaw;
        *baseYaw = playerYaw;
        return (moving || inputMoving || aiming) ? 1 : 0;
    }
    if (!moving) {
        *baseYaw = f->moveYaw;
        return 0;
    }

    stepYaw = f->haveStepYaw ? f->stepYaw : playerYaw;
    gap = mc_absf(mc_angwrap(f->fastYaw - stepYaw));
    if (f->backpedal) {
        f->backpedal = gap > MC_FOLLOW_BACKPEDAL_OFF;
    } else {
        f->backpedal = gap > MC_FOLLOW_BACKPEDAL_ON;
    }
    *baseYaw = f->backpedal ? mc_angwrap(f->moveYaw + 180.0f) : f->moveYaw;
    return 1;
}

void mc_turner_init(Mc_HeadingTurner* t) {
    t->deadzone = 10.0f;
    t->settle = 0.3f;
    t->turning = 0;
}

f32 mc_turner_update(Mc_HeadingTurner* t, f32 gap) {
    if (t->turning) {
        if (mc_absf(gap) <= t->deadzone * t->settle) {
            t->turning = 0;
        }
    } else if (mc_absf(gap) > t->deadzone) {
        t->turning = 1;
    }
    if (!t->turning) {
        return 0.0f;
    }
    return (gap > 0.0f) ? 1.0f : -1.0f;
}
