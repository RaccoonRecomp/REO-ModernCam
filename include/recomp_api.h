#ifndef __RECOMP_API_H__
#define __RECOMP_API_H__

#include "modding.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed char    s8;
typedef signed short   s16;
typedef signed int     s32;
typedef float          f32;
typedef double         f64;

// Enum options return the index of the selected option ("Off" = 0, "On" = 1).
// Number options return the slider value truncated to an integer.
RECOMP_IMPORT("*", u32 recomp_get_config_u32(const char* key));

// Number options at full precision (sensitivities, distances, times).
RECOMP_IMPORT("*", double recomp_get_config_double(const char* key));

#endif
