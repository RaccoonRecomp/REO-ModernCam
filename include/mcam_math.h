#ifndef __MCAM_MATH_H__
#define __MCAM_MATH_H__

#include "recomp_api.h"

// Single-precision math for the mod. Mods are built with -nostdinc and no libm, so the few functions
// the camera needs are here. Accuracy is around 1e-6 relative, far below anything visible on screen.

#define MC_PI      3.14159265358979f
#define MC_TWO_PI  6.28318530717959f
#define MC_HALF_PI 1.57079632679490f
#define MC_DEG2RAD 0.01745329251994f
#define MC_RAD2DEG 57.2957795130823f

typedef union {
    f32 f;
    u32 u;
    s32 s;
} Mc_FloatBits;

static inline f32 mc_absf(f32 x) {
    return x < 0.0f ? -x : x;
}

static inline f32 mc_minf(f32 a, f32 b) {
    return a < b ? a : b;
}

static inline f32 mc_maxf(f32 a, f32 b) {
    return a > b ? a : b;
}

static inline f32 mc_clampf(f32 x, f32 lo, f32 hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline f32 mc_sqrtf(f32 x) {
    return x > 0.0f ? __builtin_sqrtf(x) : 0.0f;
}

static inline f32 mc_hypotf(f32 x, f32 y) {
    return mc_sqrtf(x * x + y * y);
}

// Largest integer <= x, for |x| < 2^31.
static inline f32 mc_floorf(f32 x) {
    s32 i = (s32)x;
    f32 f = (f32)i;

    return (f > x) ? f - 1.0f : f;
}

// Python's floor-modulo: the result has the sign of m (m > 0 here).
static inline f32 mc_fmodpf(f32 x, f32 m) {
    return x - m * mc_floorf(x / m);
}

// sin on [-pi/2, pi/2] (Taylor series to x^11).
static inline f32 mc_sin_core(f32 x) {
    f32 x2 = x * x;

    return x * (1.0f + x2 * (-1.6666667e-1f + x2 * (8.3333333e-3f + x2 * (-1.9841270e-4f + x2 * (2.7557319e-6f + x2 * -2.5052108e-8f)))));
}

static inline f32 mc_sinf(f32 x) {
    // Reduce to [-pi, pi], then fold into [-pi/2, pi/2] with sin(pi - x) = sin(x).
    x = mc_fmodpf(x + MC_PI, MC_TWO_PI) - MC_PI;
    if (x > MC_HALF_PI) {
        x = MC_PI - x;
    } else if (x < -MC_HALF_PI) {
        x = -MC_PI - x;
    }
    return mc_sin_core(x);
}

static inline f32 mc_cosf(f32 x) {
    return mc_sinf(x + MC_HALF_PI);
}

// atan on [0, 1].
static inline f32 mc_atan_core(f32 x) {
    f32 x2 = x * x;

    return x * (0.99997726f + x2 * (-0.33262347f + x2 * (0.19354346f + x2 * (-0.11643287f + x2 * (0.05265332f + x2 * -0.01172120f)))));
}

// atan2 with the C library's quadrant conventions; atan2(0, 0) = 0.
static inline f32 mc_atan2f(f32 y, f32 x) {
    f32 ax = mc_absf(x);
    f32 ay = mc_absf(y);
    f32 r;

    if (ax == 0.0f && ay == 0.0f) {
        return 0.0f;
    }
    if (ay <= ax) {
        r = mc_atan_core(ay / ax);
    } else {
        r = MC_HALF_PI - mc_atan_core(ax / ay);
    }
    if (x < 0.0f) {
        r = MC_PI - r;
    }
    return (y < 0.0f) ? -r : r;
}

// e^x for x in about [-87, 88].
static inline f32 mc_expf(f32 x) {
    Mc_FloatBits scale;
    f32 k;
    f32 r;
    f32 p;

    if (x < -87.0f) {
        return 0.0f;
    }
    if (x > 88.0f) {
        x = 88.0f;
    }
    // x = k*ln2 + r, |r| <= ln2/2.
    k = mc_floorf(x * 1.44269504089f + 0.5f);
    r = x - k * 0.69314718056f;
    p = 1.0f + r * (1.0f + r * (0.5f + r * (1.6666667e-1f + r * (4.1666668e-2f + r * (8.3333338e-3f + r * 1.3888889e-3f)))));
    scale.u = (u32)((s32)k + 127) << 23;
    return p * scale.f;
}

// Natural log for x > 0 (0 or less returns a large negative number).
static inline f32 mc_logf(f32 x) {
    Mc_FloatBits b;
    s32 e;
    f32 m;
    f32 s;
    f32 s2;

    if (x <= 0.0f) {
        return -87.0f;
    }
    b.f = x;
    e = (s32)((b.u >> 23) & 0xFF) - 127;
    b.u = (b.u & 0x007FFFFFu) | 0x3F800000u; // m in [1, 2)
    m = b.f;
    if (m > 1.41421356f) {
        m *= 0.5f;
        e += 1;
    }
    // ln(m) = 2*atanh(s), s = (m-1)/(m+1), |s| < 0.172.
    s = (m - 1.0f) / (m + 1.0f);
    s2 = s * s;
    return (f32)e * 0.69314718056f + 2.0f * s * (1.0f + s2 * (0.33333333f + s2 * (0.2f + s2 * (0.14285714f + s2 * 0.11111111f))));
}

// b^e for b > 0.
static inline f32 mc_powf(f32 b, f32 e) {
    if (b <= 0.0f) {
        return 0.0f;
    }
    return mc_expf(e * mc_logf(b));
}

#endif
