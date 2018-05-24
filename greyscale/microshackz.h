// FIXME: Temporal! Add missing code for all target architectures

#ifndef _MICROS_HACKZ_H
#define _MICROS_HACKZ_H

#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)

#endif // _MICROS_HACKZ_H
