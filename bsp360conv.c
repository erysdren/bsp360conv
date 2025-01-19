
#include <SDL3/SDL.h>
#include <lzlib.h>

#define log_warning(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define log_info(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

#define BSP_MAGIC 0x50534256
#define BSP_VERSION 20
#define BSP_NUM_LUMPS 64

typedef struct bsp_lump {
	Uint32 offset;
	Uint32 length;
	Uint32 version;
	Uint32 identifier;
} bsp_lump_t;

typedef struct bsp_header {
	Uint32 magic;
	Uint32 version;
	bsp_lump_t lumps[BSP_NUM_LUMPS];
	Uint32 map_version;
} bsp_header_t;

static void read_bsp_lump(SDL_IOStream *io, bsp_lump_t *lump)
{
	SDL_ReadU32BE(io, &lump->offset);
	SDL_ReadU32BE(io, &lump->length);
	SDL_ReadU32BE(io, &lump->version);
	SDL_ReadU32BE(io, &lump->identifier);
}

static void read_bsp_header(SDL_IOStream *io, bsp_header_t *header)
{
	SDL_ReadU32BE(io, &header->magic);
	SDL_ReadU32BE(io, &header->version);
	for (int lump = 0; lump < BSP_NUM_LUMPS; lump++)
		read_bsp_lump(io, &header->lumps[lump]);
	SDL_ReadU32BE(io, &header->map_version);
}

static void write_bsp_lump(SDL_IOStream *io, bsp_lump_t *lump)
{
	SDL_WriteU32LE(io, lump->offset);
	SDL_WriteU32LE(io, lump->length);
	SDL_WriteU32LE(io, lump->version);
	SDL_WriteU32LE(io, lump->identifier);
}

static void write_bsp_header(SDL_IOStream *io, bsp_header_t *header)
{
	SDL_WriteU32LE(io, header->magic);
	SDL_WriteU32LE(io, header->version);
	for (int lump = 0; lump < BSP_NUM_LUMPS; lump++)
		write_bsp_lump(io, &header->lumps[lump]);
	SDL_WriteU32LE(io, header->map_version);
}

static bool string_endswith(const char *s, const char *e)
{
	size_t elen = SDL_strlen(e);
	size_t slen = SDL_strlen(s);
	if (elen > slen) return false;
	return SDL_strcmp(s + slen - elen, e) == 0 ? true : false;
}

static void make_output_filename(const char *input, char *output, size_t output_size)
{
	size_t inputLen = SDL_strlen(input);

	if (string_endswith(input, ".360.bsp"))
	{
		SDL_snprintf(output, output_size, "%s", input);
		SDL_snprintf(output + inputLen - 8, output_size - inputLen - 8, ".bsp", input);
	}
	else if (string_endswith(input, ".bsp"))
	{
		SDL_snprintf(output, output_size, "%s", input);
		SDL_snprintf(output + inputLen - 4, output_size - inputLen - 4, "_converted.bsp", input);
	}
	else
	{
		SDL_snprintf(output, output_size, "%s_converted.bsp", input);
	}
}

int main(int argc, char **argv)
{
	for (int arg = 1; arg < argc; arg++)
	{
		log_info("Processing \"%s\"", argv[arg]);

		/* open input file */
		SDL_IOStream *inputIo = SDL_IOFromFile(argv[arg], "rb");
		if (!inputIo)
		{
			log_warning("Failed to open \"%s\" for reading", argv[arg]);
			continue;
		}

		/* open temporary buffer for writing */
		SDL_IOStream *outputIo = SDL_IOFromDynamicMem();
		if (!outputIo)
		{
			log_warning("Failed to create output buffer");
			continue;
		}

		/* read input header */
		bsp_header_t inputHeader;
		read_bsp_header(inputIo, &inputHeader);
		if (inputHeader.magic != BSP_MAGIC || inputHeader.version != BSP_VERSION)
		{
			log_warning("\"%s\" has incorrect magic value or version", argv[arg]);
			SDL_CloseIO(inputIo);
			SDL_CloseIO(outputIo);
			continue;
		}

		/* write initial output header */
		write_bsp_header(outputIo, &inputHeader);

		/* get output filename */
		char outputFilename[1024];
		make_output_filename(argv[arg], outputFilename, sizeof(outputFilename));

		/* get pointer to the buffer we wrote */
		Sint64 outputSize = SDL_GetIOSize(outputIo);
		SDL_PropertiesID outputProps = SDL_GetIOProperties(outputIo);
		void *outputData = SDL_GetPointerProperty(outputProps, SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);

		/* save output file */
		if (!SDL_SaveFile(outputFilename, outputData, outputSize))
			log_warning("Failed to save \"%s\"", outputFilename);
		else
			log_info("Successfully Saved \"%s\"", outputFilename);

		/* clean up */
		SDL_CloseIO(inputIo);
		SDL_CloseIO(outputIo);
	}

	SDL_Quit();

	return 0;
}
