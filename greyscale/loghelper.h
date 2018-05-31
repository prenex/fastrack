#ifndef _LOG_HELPER
#define _LOG_HELPER

#include <cstdio>
// Change these according to the target architecture.
// We just use stdout and stderr as default targets.
#define  LOGD(...)  fprintf(stdout, "DEBUG:"); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n")
#define  LOGI(...)  fprintf(stdout, "INFO:"); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n")
#define  LOGW(...)  fprintf(stderr, "WARN:"); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#define  LOGE(...)  fprintf(stderr, "ERROR:"); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")

#endif // _LOG_HELPER
