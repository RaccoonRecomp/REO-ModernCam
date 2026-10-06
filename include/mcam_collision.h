#ifndef __MCAM_COLLISION_H__
#define __MCAM_COLLISION_H__

#include "mcam_math.h"

// Wall collision for the camera boom, ported from Outbreak ModernCam (ro_collision.py).

#define MC_MAX_WALLS 256

typedef struct {
    f32 minX, minZ, w, d; // XZ bounding box (w/d at least 4 units)
    f32 minY, maxY;
    f32 segX0, segZ0, segX1, segZ1; // the triangle's longest edge in the XZ plane
    u32 flags;
} Mc_Wall;

// Re-read the current room's walls if the room's wall pointer changed or the cache is older than
// a second (dt accumulates between calls). Returns the number of walls.
u32 mc_walls_refresh(f32 dt);

// Drop the cache so the next refresh goes to memory (room change, level reload).
void mc_walls_invalidate(void);

u32 mc_walls_count(void);
const Mc_Wall* mc_walls(void);

// Camera collision: intersect the boom from the head (hx, hz) to the eye (ex, ez) with the wall
// segments and pull the eye in by `margin` along the boom. Walls whose flags carry ignoreFlags are
// skipped, and when useMinTop is set so are walls whose top is below minTop. With useHy the eye's
// height is interpolated from hy as well. Returns t in (0, 1]: the fraction of the boom kept.
f32 mc_clamp_eye_to_walls(f32* ex, f32* ez, f32* ey, f32 hx, f32 hz, f32 margin, u32 ignoreFlags, u32 useHy, f32 hy,
                          u32 useMinTop, f32 minTop);

#endif
