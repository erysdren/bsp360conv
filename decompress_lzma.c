
#include <SDL3/SDL.h>
#include <lzma.h>

#include "decompress_lzma.h"
#include "utils.h"

#define LZMA_MAGIC 0x414d5a4c

typedef struct lzma_header {
	Uint8 properties;
	Uint32 dictionary_size;
	Uint64 uncompressed_size;
} __attribute__((packed)) lzma_header_t;

typedef struct lzma_source_header {
	Uint32 magic;
	Uint32 uncompressed_size;
	Uint32 compressed_size;
	Uint8 properties;
	Uint32 dictionary_size;
} lzma_source_header_t;

void *decompress_lzma(SDL_IOStream *io, Sint64 *size)
{
	/* validate magic */
	lzma_source_header_t source_header;
	SDL_ReadU32LE(io, &source_header.magic);
	if (source_header.magic != LZMA_MAGIC)
	{
		log_warning("Buffer is not LZMA");
		SDL_SeekIO(io, -4, SDL_IO_SEEK_CUR);
		return NULL;
	}

	/* read the rest of the header */
	SDL_ReadU32LE(io, &source_header.uncompressed_size);
	SDL_ReadU32LE(io, &source_header.compressed_size);
	SDL_ReadU8(io, &source_header.properties);
	SDL_ReadU32LE(io, &source_header.dictionary_size);

	/* allocate buffers */
	void *compressed = SDL_malloc(sizeof(lzma_header_t) + source_header.compressed_size);
	void *uncompressed = SDL_malloc(source_header.uncompressed_size);

	/* setup compressed header */
	lzma_header_t *header = (lzma_header_t *)compressed;
	header->properties = source_header.properties;
	header->dictionary_size = source_header.dictionary_size;
	header->uncompressed_size = (Uint64)source_header.uncompressed_size;

	/* read compressed data */
	SDL_ReadIO(io, (Uint8 *)compressed + sizeof(lzma_header_t), source_header.compressed_size);

	/* open decoder */
	lzma_stream decoder = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_alone_decoder(&decoder, UINT64_MAX);
	if (ret != LZMA_OK)
	{
		log_warning("Failed to initialize LZMA decoder");
		SDL_free(compressed);
		SDL_free(uncompressed);
		return NULL;
	}

	/* initialize decoder */
	decoder.next_in = compressed;
	decoder.avail_in = sizeof(lzma_header_t) + source_header.compressed_size;
	decoder.next_out = uncompressed;
	decoder.avail_out = source_header.uncompressed_size;

	/* do decompression */
	bool error = false;
	while (1)
	{
		ret = lzma_code(&decoder, LZMA_RUN);

		if (decoder.avail_out == 0 || ret == LZMA_STREAM_END)
		{
			break;
		}
		else if (ret != LZMA_OK)
		{
			log_warning("Failed to decompress LZMA buffer");
			error = true;
			break;
		}
	}

	/* clean up */
	lzma_end(&decoder);
	SDL_free(compressed);

	/* there was an error */
	if (error)
	{
		SDL_free(uncompressed);
		return NULL;
	}

	if (size) *size = source_header.uncompressed_size;
	return uncompressed;
}
