#ifndef __MCAM_MEM_H__
#define __MCAM_MEM_H__

#include "recomp_api.h"

// EE RAM access for the camera mod (same scheme as the REO cheat mods).
//
// EE RAM mapping: an aligned EE word (0x00000000-0x01FFFFFF) is dereferenced directly. If mod code
// sees EE RAM at a different base, MC_EE_U32 is the only thing that changes.
#define MC_EE_U32(addr) (*(volatile u32*)(addr))

#define MC_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

// EE RAM is little-endian, but N64Recomp translates a mod's byte and halfword loads/stores with
// big-endian (N64) byte order, so they would land on the wrong bytes of the word. Every access goes
// through the aligned 32-bit word, and bytes/halfwords are shifted in and out of it.
// addr must be naturally aligned for its width.

static inline u32 Mc_ReadU32(u32 addr) {
    return MC_EE_U32(addr & ~3u);
}

static inline void Mc_WriteU32(u32 addr, u32 value) {
    MC_EE_U32(addr & ~3u) = value;
}

static inline u32 Mc_ReadU8(u32 addr) {
    return (MC_EE_U32(addr & ~3u) >> ((addr & 3u) * 8)) & 0xFFu;
}

static inline u32 Mc_ReadU16(u32 addr) {
    return (MC_EE_U32(addr & ~3u) >> ((addr & 2u) * 8)) & 0xFFFFu;
}

static inline s32 Mc_ReadS16(u32 addr) {
    return (s32)(s16)Mc_ReadU16(addr);
}

static inline void Mc_WriteU8(u32 addr, u32 value) {
    u32 shift = (addr & 3u) * 8;
    u32 mask = 0xFFu << shift;

    MC_EE_U32(addr & ~3u) = (MC_EE_U32(addr & ~3u) & ~mask) | ((value << shift) & mask);
}

static inline void Mc_WriteU16(u32 addr, u32 value) {
    u32 shift = (addr & 2u) * 8;
    u32 mask = 0xFFFFu << shift;

    MC_EE_U32(addr & ~3u) = (MC_EE_U32(addr & ~3u) & ~mask) | ((value << shift) & mask);
}

typedef union {
    f32 f;
    u32 u;
} Mc_Word;

static inline f32 Mc_ReadF32(u32 addr) {
    Mc_Word w;

    w.u = Mc_ReadU32(addr);
    return w.f;
}

static inline void Mc_WriteF32(u32 addr, f32 value) {
    Mc_Word w;

    w.f = value;
    Mc_WriteU32(addr, w.u);
}

// Finite, tested on the bits: the mod is built with -ffast-math, which lets the compiler assume
// there are no NaNs and drop a `v == v` test.
static inline u32 Mc_IsFiniteF32(f32 v) {
    Mc_Word w;

    w.f = v;
    return (w.u & 0x7F800000u) != 0x7F800000u;
}

// A float that is safe to use: finite and not absurdly large.
static inline u32 Mc_IsSaneF32(f32 v) {
    return Mc_IsFiniteF32(v) && v > -1e6f && v < 1e6f;
}

#endif
