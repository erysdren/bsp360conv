
#include <SDL3/SDL.h>

#include "decompress_lzma.h"
#include "utils.h"

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

typedef struct vector {
	float x;
	float y;
	float z;
} vector_t;

typedef struct plane {
	vector_t normal;
	float dist;
	Sint32 type;
} plane_t;

typedef struct edge {
	Uint16 indices[2];
} edge_t;

typedef struct brush {
	Sint32 first_side;
	Sint32 num_sides;
	Sint32 contents;
} brush_t;

typedef struct brushside {
	Uint16 plane_num;
	Sint16 tex_info;
	Sint16 disp_info;
	Sint16 bevel;
} brushside_t;

typedef struct texdata {
	vector_t reflectivity;
	Sint32 name_index;
	Sint32 width, height;
	Sint32 view_width, view_height;
} texdata_t;

typedef struct texinfo {
	float texture_vectors[2][4];
	float lightmap_vectors[2][4];
	Sint32 flags;
	Sint32 tex_data;
} texinfo_t;

typedef struct node {
	Sint32 plane_num;
	Sint32 children[2];
	Sint16 mins[3];
	Sint16 maxs[3];
	Uint16 first_face;
	Uint16 num_faces;
	Sint16 area;
	Sint16 pad;
} node_t;

static bool swap_lump(int lump, void *lump_data, Sint64 lump_size)
{
#define CHECK_FUNNY_LUMP_SIZE(s) if (lump_size % s != 0) return false;
	switch (lump)
	{
		/* entities */
		case 0:
		{
			return true;
		}

		/* planes */
		case 1:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(plane_t));

			plane_t *planes = (plane_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(plane_t); i++)
			{
				planes[i].normal.x = SDL_SwapFloat(planes[i].normal.x);
				planes[i].normal.y = SDL_SwapFloat(planes[i].normal.y);
				planes[i].normal.z = SDL_SwapFloat(planes[i].normal.z);
				planes[i].dist = SDL_SwapFloat(planes[i].dist);
				planes[i].type = SDL_Swap32(planes[i].type);
			}

			return true;
		}

		/* texdata */
		case 2:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(texdata_t));

			texdata_t *texdata = (texdata_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(texdata_t); i++)
			{
				texdata[i].reflectivity.x = SDL_SwapFloat(texdata[i].reflectivity.x);
				texdata[i].reflectivity.y = SDL_SwapFloat(texdata[i].reflectivity.y);
				texdata[i].reflectivity.z = SDL_SwapFloat(texdata[i].reflectivity.z);
				texdata[i].name_index = SDL_Swap32(texdata[i].name_index);
				texdata[i].width = SDL_Swap32(texdata[i].width);
				texdata[i].height = SDL_Swap32(texdata[i].height);
				texdata[i].view_width = SDL_Swap32(texdata[i].view_width);
				texdata[i].view_height = SDL_Swap32(texdata[i].view_height);
			}

			return true;
		}

		/* vertices */
		case 3:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(vector_t));

			vector_t *vertices = (vector_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(vector_t); i++)
			{
				vertices[i].x = SDL_SwapFloat(vertices[i].x);
				vertices[i].y = SDL_SwapFloat(vertices[i].y);
				vertices[i].z = SDL_SwapFloat(vertices[i].z);
			}

			return true;
		}

		/* vis */
		case 4:
		{
			Sint32 *vis = (Sint32 *)lump_data;
			vis[0] = SDL_Swap32(vis[0]);
			for (int i = 0; i < vis[0] * 2; i++)
				vis[i + 1] = SDL_Swap32(vis[i + 1]);

			return true;
		}

		/* nodes */
		case 5:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(node_t));

			node_t *nodes = (node_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(node_t); i++)
			{
				nodes[i].plane_num = SDL_Swap32(nodes[i].plane_num);
				nodes[i].children[0] = SDL_Swap32(nodes[i].children[0]);
				nodes[i].children[1] = SDL_Swap32(nodes[i].children[1]);
				nodes[i].mins[0] = SDL_Swap16(nodes[i].mins[0]);
				nodes[i].mins[1] = SDL_Swap16(nodes[i].mins[1]);
				nodes[i].mins[2] = SDL_Swap16(nodes[i].mins[2]);
				nodes[i].maxs[0] = SDL_Swap16(nodes[i].maxs[0]);
				nodes[i].maxs[1] = SDL_Swap16(nodes[i].maxs[1]);
				nodes[i].maxs[2] = SDL_Swap16(nodes[i].maxs[2]);
				nodes[i].first_face = SDL_Swap16(nodes[i].first_face);
				nodes[i].num_faces = SDL_Swap16(nodes[i].num_faces);
				nodes[i].area = SDL_Swap16(nodes[i].area);
				nodes[i].pad = SDL_Swap16(nodes[i].pad);
			}

			return true;
		}

		/* texinfos */
		case 6:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(texinfo_t));

			texinfo_t *texinfos = (texinfo_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(texinfo_t); i++)
			{
				for (int j = 0; j < 2; j++)
				{
					for (int k = 0; k < 4; k++)
					{
						texinfos[i].texture_vectors[j][k] = SDL_SwapFloat(texinfos[i].texture_vectors[j][k]);
						texinfos[i].lightmap_vectors[j][k] = SDL_SwapFloat(texinfos[i].lightmap_vectors[j][k]);
					}
				}

				texinfos[i].flags = SDL_Swap32(texinfos[i].flags);
				texinfos[i].tex_data = SDL_Swap32(texinfos[i].tex_data);
			}

			return true;
		}

		/* edges */
		case 12:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(edge_t));

			edge_t *edges = (edge_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(edge_t); i++)
			{
				edges[i].indices[0] = SDL_Swap16(edges[i].indices[0]);
				edges[i].indices[1] = SDL_Swap16(edges[i].indices[1]);
			}

			return true;
		}

		/* surfedges */
		case 13:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(Sint32));

			Sint32 *surfedges = (Sint32 *)lump_data;
			for (int i = 0; i < lump_size / sizeof(Sint32); i++)
				surfedges[i] = SDL_Swap32(surfedges[i]);

			return true;
		}

		/* brushes */
		case 18:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(brush_t));

			brush_t *brushes = (brush_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(brush_t); i++)
			{
				brushes[i].first_side = SDL_Swap32(brushes[i].first_side);
				brushes[i].num_sides = SDL_Swap32(brushes[i].num_sides);
				brushes[i].contents = SDL_Swap32(brushes[i].contents);
			}

			return true;
		}

		/* brushsides */
		case 19:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(brushside_t));

			brushside_t *brushsides = (brushside_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(brushside_t); i++)
			{
				brushsides[i].plane_num = SDL_Swap16(brushsides[i].plane_num);
				brushsides[i].tex_info = SDL_Swap16(brushsides[i].tex_info);
				brushsides[i].disp_info = SDL_Swap16(brushsides[i].disp_info);
				brushsides[i].bevel = SDL_Swap16(brushsides[i].bevel);
			}

			return true;
		}

		/* unknown lump */
		default:
		{
			return false;
		}
	}
#undef CHECK_FUNNY_LUMP_SIZE
}

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
			goto cleanup;
		}

		/* open temporary buffer for writing */
		SDL_IOStream *outputIo = SDL_IOFromDynamicMem();
		if (!outputIo)
		{
			log_warning("Failed to create output buffer");
			goto cleanup;
		}

		/* read input header */
		bsp_header_t inputHeader;
		read_bsp_header(inputIo, &inputHeader);
		if (inputHeader.magic != BSP_MAGIC || inputHeader.version != BSP_VERSION)
		{
			log_warning("\"%s\" has incorrect magic value or version", argv[arg]);
			goto cleanup;
		}

		/* write initial output header */
		write_bsp_header(outputIo, &inputHeader);

		/* rewrite all lumps, decompress if needed */
		for (int lump = 0; lump < BSP_NUM_LUMPS; lump++)
		{
			/* seek to lump data position */
			SDL_SeekIO(inputIo, inputHeader.lumps[lump].offset, SDL_IO_SEEK_SET);

			/* they use the identifier to show that its compressed... for some reason */
			if (inputHeader.lumps[lump].identifier > 0)
			{
				/* decompress lzma stuff */
				Sint64 uncompressed_size = -1;
				void *uncompressed = decompress_lzma(inputIo, &uncompressed_size);

				/* catch errors */
				if (uncompressed == NULL)
				{
					log_warning("Lump %d: Failed to decompress", lump);
					goto cleanup;
				}
				else if (inputHeader.lumps[lump].identifier != uncompressed_size)
				{
					SDL_free(uncompressed);
					log_warning("Lump %d: Size mismatch %d != %d", lump, inputHeader.lumps[lump].identifier, uncompressed_size);
					goto cleanup;
				}

				/* save new offset and size */
				inputHeader.lumps[lump].offset = SDL_TellIO(outputIo);
				inputHeader.lumps[lump].length = uncompressed_size;

				/* byteswap data */
				if (!swap_lump(lump, uncompressed, uncompressed_size))
					log_warning("Lump %d: Failed to byteswap data", lump);

				/* write lump data */
				SDL_WriteIO(outputIo, uncompressed, uncompressed_size);

				/* clean up */
				SDL_free(uncompressed);
			}
			else if (inputHeader.lumps[lump].length > 0)
			{
				/* read lump data */
				void *lump_data = SDL_malloc(inputHeader.lumps[lump].length);
				SDL_ReadIO(inputIo, lump_data, inputHeader.lumps[lump].length);

				/* save new offset */
				inputHeader.lumps[lump].offset = SDL_TellIO(outputIo);

				/* byteswap data */
				if (!swap_lump(lump, lump_data, inputHeader.lumps[lump].length))
					log_warning("Lump %d: Failed to byteswap data", lump);

				/* write lump data */
				SDL_WriteIO(outputIo, lump_data, inputHeader.lumps[lump].length);

				/* clean up */
				SDL_free(lump_data);
			}
		}

		/* rewrite output header */
		SDL_SeekIO(outputIo, 0, SDL_IO_SEEK_SET);
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
cleanup:
		if (inputIo) SDL_CloseIO(inputIo);
		if (outputIo) SDL_CloseIO(outputIo);
	}

	SDL_Quit();

	return 0;
}
