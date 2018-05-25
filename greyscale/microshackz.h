// This file contains various "hackz" and "micros" for optimization purposes

#ifndef _MICROS_HACKZ_H
#define _MICROS_HACKZ_H

#ifdef _MSC_VER
	// MSVC++
#define RESTRICT __restrict
#define LIKELY(x)      x
#define UNLIKELY(x)    x
#else
	// Usual clang++ or g++ or even em++
#define RESTRICT __restrict__
#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)
#endif


#endif // _MICROS_HACKZ_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
