#include "mcam_collision.h"
#include "reo_camera_us.h"
#include "mcam_mem.h"

// Ported from Outbreak ModernCam (ro_collision.py). The wall record layout and the filters are the
// original's; only the pointer to the room's wall array is this build's (see reo_camera_us.h).

#define MC_WALL_STRIDE      0x38 // 56-byte records
#define MC_WALL_REFRESH_S   1.0f
#define MC_WALL_MIN_HEIGHT  20.0f
#define MC_WALL_MAX_NY      0.707f // mostly horizontal (floor or ceiling): not a wall
#define MC_BOOM_MIN_KEEP    0.15f

static Mc_Wall sWalls[MC_MAX_WALLS];
static u32 sWallCount = 0;
static u32 sWallBase = 0;
static u32 sHaveCache = 0;
static f32 sWallAge = 0.0f;

static f32 min3(f32 a, f32 b, f32 c) {
    return mc_minf(a, mc_minf(b, c));
}

static f32 max3(f32 a, f32 b, f32 c) {
    return mc_maxf(a, mc_maxf(b, c));
}

void mc_walls_invalidate(void) {
    sHaveCache = 0;
}

u32 mc_walls_count(void) {
    return sWallCount;
}

const Mc_Wall* mc_walls(void) {
    return sWalls;
}

u32 mc_walls_refresh(f32 dt) {
    u32 base = Mc_ReadU32(REO_US_WALL_PTR);
    u32 i;

    sWallAge += dt;
    if (sHaveCache && base == sWallBase && sWallAge < MC_WALL_REFRESH_S) {
        return sWallCount;
    }
    sHaveCache = 1;
    sWallBase = base;
    sWallAge = 0.0f;
    sWallCount = 0;

    if (!(base > 0x100000 && base < 0x2000000)) {
        return 0;
    }

    for (i = 0; i < MC_MAX_WALLS; i++) {
        u32 rec = base + i * MC_WALL_STRIDE;
        u32 flags;
        f32 x0, y0, z0, x1, y1, z1, x2, y2, z2, ny;
        f32 w, d, d01, d12, d20;
        Mc_Wall* wall;

        if (rec + MC_WALL_STRIDE > 0x2000000) {
            break;
        }
        flags = Mc_ReadU32(rec);
        if (flags == 0 || flags == 0xFFFFFFFFu) {
            break; // end of the list
        }
        x0 = Mc_ReadF32(rec + 0x04);
        y0 = Mc_ReadF32(rec + 0x08);
        z0 = Mc_ReadF32(rec + 0x0C);
        x1 = Mc_ReadF32(rec + 0x10);
        y1 = Mc_ReadF32(rec + 0x14);
        z1 = Mc_ReadF32(rec + 0x18);
        x2 = Mc_ReadF32(rec + 0x1C);
        y2 = Mc_ReadF32(rec + 0x20);
        z2 = Mc_ReadF32(rec + 0x24);
        ny = Mc_ReadF32(rec + 0x2C);

        if (mc_absf(ny) > MC_WALL_MAX_NY) {
            continue;
        }

        wall = &sWalls[sWallCount];
        wall->minX = min3(x0, x1, x2);
        wall->minZ = min3(z0, z1, z2);
        wall->minY = min3(y0, y1, y2);
        wall->maxY = max3(y0, y1, y2);
        w = max3(x0, x1, x2) - wall->minX;
        d = max3(z0, z1, z2) - wall->minZ;

        if (wall->maxY - wall->minY < MC_WALL_MIN_HEIGHT) {
            continue;
        }
        if (w < 1.0f) { // axis-aligned sliver: give it 4 units of thickness
            wall->minX -= 2.0f;
            w = 4.0f;
        }
        if (d < 1.0f) {
            wall->minZ -= 2.0f;
            d = 4.0f;
        }
        wall->w = w;
        wall->d = d;

        // The wall segment is the triangle's longest edge in the XZ plane.
        d01 = (x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0);
        d12 = (x2 - x1) * (x2 - x1) + (z2 - z1) * (z2 - z1);
        d20 = (x0 - x2) * (x0 - x2) + (z0 - z2) * (z0 - z2);
        if (d01 >= d12 && d01 >= d20) {
            wall->segX0 = x0; wall->segZ0 = z0; wall->segX1 = x1; wall->segZ1 = z1;
        } else if (d12 >= d01 && d12 >= d20) {
            wall->segX0 = x1; wall->segZ0 = z1; wall->segX1 = x2; wall->segZ1 = z2;
        } else {
            wall->segX0 = x2; wall->segZ0 = z2; wall->segX1 = x0; wall->segZ1 = z0;
        }
        wall->flags = flags;
        sWallCount++;
    }
    return sWallCount;
}

f32 mc_clamp_eye_to_walls(f32* ex, f32* ez, f32* ey, f32 hx, f32 hz, f32 margin, u32 ignoreFlags, u32 useHy, f32 hy,
                          u32 useMinTop, f32 minTop) {
    f32 minT = 1.0f;
    u32 i;

    for (i = 0; i < sWallCount; i++) {
        const Mc_Wall* w = &sWalls[i];
        f32 den, t, u, yInt;

        if (ignoreFlags && (w->flags & ignoreFlags)) {
            continue;
        }
        if (useMinTop && w->maxY < minTop) {
            continue;
        }
        den = (hx - *ex) * (w->segZ0 - w->segZ1) - (hz - *ez) * (w->segX0 - w->segX1);
        if (den == 0.0f) {
            continue;
        }
        t = ((hx - w->segX0) * (w->segZ0 - w->segZ1) - (hz - w->segZ0) * (w->segX0 - w->segX1)) / den;
        u = -((hx - *ex) * (hz - w->segZ0) - (hz - *ez) * (hx - w->segX0)) / den;

        // t runs from the head (0) toward the eye (1).
        if (t >= 0.0f && t < minT && u >= -0.01f && u <= 1.01f) {
            yInt = useHy ? hy + (*ey - hy) * t : *ey;
            if (yInt >= w->minY && yInt <= w->maxY) {
                minT = t;
            }
        }
    }

    if (minT < 1.0f) {
        f32 len = mc_hypotf(*ex - hx, *ez - hz);

        if (len > 0.0f) {
            minT = mc_maxf(MC_BOOM_MIN_KEEP, minT - margin / len);
        }
        if (useHy) {
            *ey = hy + (*ey - hy) * minT;
        }
        *ex = hx + (*ex - hx) * minT;
        *ez = hz + (*ez - hz) * minT;
        return minT;
    }
    return 1.0f;
}
