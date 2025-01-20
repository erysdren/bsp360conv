
#include "utils.h"

bool string_endswith(const char *s, const char *e)
{
	size_t elen = SDL_strlen(e);
	size_t slen = SDL_strlen(s);
	if (elen > slen) return false;
	return SDL_strcmp(s + slen - elen, e) == 0 ? true : false;
}
