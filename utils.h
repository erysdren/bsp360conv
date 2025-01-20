
#ifndef _UTILS_H_
#define _UTILS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <SDL3/SDL.h>

#define log_warning(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define log_info(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

bool string_endswith(const char *s, const char *e);

#ifdef __cplusplus
}
#endif
#endif /* _UTILS_H_ */
