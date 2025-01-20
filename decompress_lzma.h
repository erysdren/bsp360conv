
#ifndef _DECOMPRESS_LZMA_H_
#define _DECOMPRESS_LZMA_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <SDL3/SDL.h>

/**
 * \brief decompress an LZMA buffer from the current point in the IOStream
 *
 * \param io the IOStream to read from
 * \param size pointer to fill with the size of the decompressed buffer
 *
 * \author erysdren (it/its)
 *
 * \returns the decompressed buffer, or NULL on error
 *
 * \note return buffer must be freed with SDL_free()
 */
void *decompress_lzma(SDL_IOStream *io, Sint64 *size);

#ifdef __cplusplus
}
#endif
#endif /* _DECOMPRESS_LZMA_H_ */
