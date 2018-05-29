#ifndef _MICROS_HACKZ_H
#define _MICROS_HACKZ_H

#ifdef _MSC_VER
	// MSVC++
#define RESTRICT       __restrict
#define LIKELY(x)      x
#define UNLIKELY(x)    x
#define NOINLINE       __declspec(noinline)
#else
	// Usual clang++ or g++ or even em++
#define RESTRICT       __restrict__
#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)
#define NOINLINE       __attribute__ ((noinline))
#endif


#endif // _MICROS_HACKZ_H
