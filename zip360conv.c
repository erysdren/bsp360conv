
#include <SDL3/SDL.h>

#include "decompress_lzma.h"
#include "utils.h"

#define ZIP_MAGIC_SIGNATURE 0x4b50
#define ZIP_MAGIC_CENTRAL_DIR_ENTRY 0x0201
#define ZIP_MAGIC_LOCAL_FILE_HEADER 0x0403
#define ZIP_MAGIC_CENTRAL_DIR_END 0x0605

typedef struct zip_central_dir_end {
	Uint16 signature;
	Uint16 type;
	Uint16 disk;
	Uint16 disk_with_central_dir;
	Uint16 num_entries_this_disk;
	Uint16 num_entries_total;
	Uint32 len_directory;
	Uint32 ofs_directory;
	Uint16 len_comment;
	char *comment;
} zip_central_dir_end_t;

typedef struct zip_local_file_header {
	Uint16 signature;
	Uint16 type;
	Uint16 version_needed;
	Uint16 flags;
	Uint16 compression;
	Uint16 file_time;
	Uint16 file_date;
	Uint32 crc32;
	Uint32 len_file_compressed;
	Uint32 len_file_uncompressed;
	Uint16 len_filename;
	Uint16 len_extra;
	char *filename;
	void *extra;
	void *data;
} zip_local_file_header_t;

typedef struct zip_central_dir_entry {
	Uint16 signature;
	Uint16 type;
	Uint16 version_made_with;
	Uint16 version_needed;
	Uint16 flags;
	Uint16 compression;
	Uint16 file_time;
	Uint16 file_date;
	Uint32 crc32;
	Uint32 len_file_compressed;
	Uint32 len_file_uncompressed;
	Uint16 len_filename;
	Uint16 len_extra;
	Uint16 len_comment;
	Uint16 disk;
	Uint16 internal_attributes;
	Uint32 external_attributes;
	Uint32 ofs_local_file_header;
	char *filename;
	void *extra;
	char *comment;
	zip_local_file_header_t local_file_header;
} zip_central_dir_entry_t;

static void make_output_filename(const char *input, char *output, size_t output_size)
{
	size_t inputLen = SDL_strlen(input);

	if (string_endswith(input, ".360.zip"))
	{
		SDL_snprintf(output, output_size, "%s", input);
		SDL_snprintf(output + inputLen - 8, output_size - inputLen - 8, ".zip", input);
	}
	else if (string_endswith(input, ".zip"))
	{
		SDL_snprintf(output, output_size, "%s", input);
		SDL_snprintf(output + inputLen - 4, output_size - inputLen - 4, "_converted.zip", input);
	}
	else
	{
		SDL_snprintf(output, output_size, "%s_converted.zip", input);
	}
}

static void read_central_dir_entry(SDL_IOStream *io, zip_central_dir_entry_t *entry)
{
	SDL_ReadU16LE(io, &entry->signature);
	SDL_ReadU16LE(io, &entry->type);
	SDL_ReadU16LE(io, &entry->version_made_with);
	SDL_ReadU16LE(io, &entry->version_needed);
	SDL_ReadU16LE(io, &entry->flags);
	SDL_ReadU16LE(io, &entry->compression);
	SDL_ReadU16LE(io, &entry->file_time);
	SDL_ReadU16LE(io, &entry->file_date);
	SDL_ReadU32LE(io, &entry->crc32);
	SDL_ReadU32LE(io, &entry->len_file_compressed);
	SDL_ReadU32LE(io, &entry->len_file_uncompressed);
	SDL_ReadU16LE(io, &entry->len_filename);
	SDL_ReadU16LE(io, &entry->len_extra);
	SDL_ReadU16LE(io, &entry->len_comment);
	SDL_ReadU16LE(io, &entry->disk);
	SDL_ReadU16LE(io, &entry->internal_attributes);
	SDL_ReadU32LE(io, &entry->external_attributes);
	SDL_ReadU32LE(io, &entry->ofs_local_file_header);

	if (entry->len_filename)
	{
		entry->filename = SDL_malloc(entry->len_filename + 1);
		entry->filename[entry->len_filename] = '\0';
		SDL_ReadIO(io, entry->filename, entry->len_filename);
	}
	else
	{
		entry->filename = NULL;
	}

#if 0
	if (entry->len_extra)
	{
		entry->extra = SDL_malloc(entry->len_extra);
		SDL_ReadIO(io, entry->extra, entry->len_extra);
	}
	else
	{
		entry->extra = NULL;
	}

	if (entry->len_comment)
	{
		entry->comment = SDL_malloc(entry->len_comment + 1);
		entry->comment[entry->len_comment] = '\0';
		SDL_ReadIO(io, entry->comment, entry->len_comment);
	}
	else
	{
		entry->comment = NULL;
	}
#else
	entry->extra = NULL;
	entry->comment = NULL;
#endif
}

static void write_central_dir_entry(SDL_IOStream *io, zip_central_dir_entry_t *entry)
{
	SDL_WriteU16LE(io, entry->signature);
	SDL_WriteU16LE(io, entry->type);
	SDL_WriteU16LE(io, entry->version_made_with);
	SDL_WriteU16LE(io, entry->version_needed);
	SDL_WriteU16LE(io, entry->flags);
	SDL_WriteU16LE(io, entry->compression);
	SDL_WriteU16LE(io, entry->file_time);
	SDL_WriteU16LE(io, entry->file_date);
	SDL_WriteU32LE(io, entry->crc32);
	SDL_WriteU32LE(io, entry->len_file_compressed);
	SDL_WriteU32LE(io, entry->len_file_uncompressed);
	SDL_WriteU16LE(io, entry->len_filename);
	SDL_WriteU16LE(io, entry->len_extra);
	SDL_WriteU16LE(io, entry->len_comment);
	SDL_WriteU16LE(io, entry->disk);
	SDL_WriteU16LE(io, entry->internal_attributes);
	SDL_WriteU32LE(io, entry->external_attributes);
	SDL_WriteU32LE(io, entry->ofs_local_file_header);

	/* fix up filename */
	for (int i = 0; i < entry->len_filename; i++)
		if (entry->filename[i] == '\\')
			entry->filename[i] = '/';

	if (entry->len_filename) SDL_WriteIO(io, entry->filename, entry->len_filename);
	if (entry->len_extra) SDL_WriteIO(io, entry->extra, entry->len_extra);
	if (entry->len_comment) SDL_WriteIO(io, entry->comment, entry->len_comment);
}

static void read_local_file_header(SDL_IOStream *io, zip_local_file_header_t *header)
{
	SDL_ReadU16LE(io, &header->signature);
	SDL_ReadU16LE(io, &header->type);
	SDL_ReadU16LE(io, &header->version_needed);
	SDL_ReadU16LE(io, &header->flags);
	SDL_ReadU16LE(io, &header->compression);
	SDL_ReadU16LE(io, &header->file_time);
	SDL_ReadU16LE(io, &header->file_date);
	SDL_ReadU32LE(io, &header->crc32);
	SDL_ReadU32LE(io, &header->len_file_compressed);
	SDL_ReadU32LE(io, &header->len_file_uncompressed);
	SDL_ReadU16LE(io, &header->len_filename);
	SDL_ReadU16LE(io, &header->len_extra);

	if (header->len_filename)
	{
		header->filename = SDL_malloc(header->len_filename + 1);
		header->filename[header->len_filename] = '\0';
		SDL_ReadIO(io, header->filename, header->len_filename);
	}
	else
	{
		header->filename = NULL;
	}

	if (header->len_extra)
	{
		header->extra = SDL_malloc(header->len_extra + 1);
		SDL_ReadIO(io, header->extra, header->len_extra);
	}
	else
	{
		header->extra = NULL;
	}

	header->data = SDL_malloc(header->len_file_compressed);
	SDL_ReadIO(io, header->data, header->len_file_compressed);
}

static void write_local_file_header(SDL_IOStream *io, zip_local_file_header_t *header)
{
	SDL_WriteU16LE(io, header->signature);
	SDL_WriteU16LE(io, header->type);
	SDL_WriteU16LE(io, header->version_needed);
	SDL_WriteU16LE(io, header->flags);
	SDL_WriteU16LE(io, header->compression);
	SDL_WriteU16LE(io, header->file_time);
	SDL_WriteU16LE(io, header->file_date);
	SDL_WriteU32LE(io, header->crc32);
	SDL_WriteU32LE(io, header->len_file_compressed);
	SDL_WriteU32LE(io, header->len_file_uncompressed);
	SDL_WriteU16LE(io, header->len_filename);
	SDL_WriteU16LE(io, header->len_extra);

	/* fix up filename */
	for (int i = 0; i < header->len_filename; i++)
		if (header->filename[i] == '\\')
			header->filename[i] = '/';

	if (header->len_filename) SDL_WriteIO(io, header->filename, header->len_filename);
	if (header->len_extra) SDL_WriteIO(io, header->extra, header->len_extra);
	if (header->len_file_compressed) SDL_WriteIO(io, header->data, header->len_file_compressed);
}

static void read_central_dir_end(SDL_IOStream *io, zip_central_dir_end_t *central_dir_end)
{
	SDL_ReadU16LE(io, &central_dir_end->signature);
	SDL_ReadU16LE(io, &central_dir_end->type);
	SDL_ReadU16LE(io, &central_dir_end->disk);
	SDL_ReadU16LE(io, &central_dir_end->disk_with_central_dir);
	SDL_ReadU16LE(io, &central_dir_end->num_entries_this_disk);
	SDL_ReadU16LE(io, &central_dir_end->num_entries_total);
	SDL_ReadU32LE(io, &central_dir_end->len_directory);
	SDL_ReadU32LE(io, &central_dir_end->ofs_directory);
	SDL_ReadU16LE(io, &central_dir_end->len_comment);

	if (central_dir_end->len_comment)
	{
		central_dir_end->comment = SDL_malloc(central_dir_end->len_comment + 1);
		central_dir_end->comment[central_dir_end->len_comment] = '\0';
		SDL_ReadIO(io, central_dir_end->comment, central_dir_end->len_comment);
	}
	else
	{
		central_dir_end->comment = NULL;
	}
}

static void write_central_dir_end(SDL_IOStream *io, zip_central_dir_end_t *central_dir_end)
{
	SDL_WriteU16LE(io, central_dir_end->signature);
	SDL_WriteU16LE(io, central_dir_end->type);
	SDL_WriteU16LE(io, central_dir_end->disk);
	SDL_WriteU16LE(io, central_dir_end->disk_with_central_dir);
	SDL_WriteU16LE(io, central_dir_end->num_entries_this_disk);
	SDL_WriteU16LE(io, central_dir_end->num_entries_total);
	SDL_WriteU32LE(io, central_dir_end->len_directory);
	SDL_WriteU32LE(io, central_dir_end->ofs_directory);
	SDL_WriteU16LE(io, central_dir_end->len_comment);
	SDL_WriteIO(io, central_dir_end->comment, central_dir_end->len_comment);
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

		/* get end of central dir record */
		/* NOTE: assumes comment length of 32 bytes, which xbox 360 zip use */
		zip_central_dir_end_t central_dir_end;
		SDL_SeekIO(inputIo, -54, SDL_IO_SEEK_END);
		read_central_dir_end(inputIo, &central_dir_end);

		/* validate magic */
		if (central_dir_end.signature != ZIP_MAGIC_SIGNATURE || central_dir_end.type != ZIP_MAGIC_CENTRAL_DIR_END)
		{
			log_warning("Failed to validate \"%s\" as an Xbox 360 zip file", argv[arg]);
			goto cleanup;
		}

		/* validate disk numbers */
		if (central_dir_end.disk != central_dir_end.disk_with_central_dir || central_dir_end.num_entries_this_disk != central_dir_end.num_entries_total)
		{
			log_warning("Multi-part zips are not supported");
			goto cleanup;
		}

		/* read central dir entries */
		SDL_SeekIO(inputIo, central_dir_end.ofs_directory, SDL_IO_SEEK_SET);
		zip_central_dir_entry_t *entries = SDL_malloc(sizeof(zip_central_dir_entry_t) * central_dir_end.num_entries_total);
		for (int entry = 0; entry < central_dir_end.num_entries_total; entry++)
		{
			read_central_dir_entry(inputIo, &entries[entry]);

			if (entries[entry].signature != ZIP_MAGIC_SIGNATURE || entries[entry].type != ZIP_MAGIC_CENTRAL_DIR_ENTRY)
			{
				log_warning("Central directory entry %d failed to validate", entry);
				goto cleanup;
			}

			if (entries[entry].compression != 0)
			{
				log_warning("Compressed files are not supported");
				goto cleanup;
			}
		}

		/* read files */
		for (int entry = 0; entry < central_dir_end.num_entries_total; entry++)
		{
			SDL_SeekIO(inputIo, entries[entry].ofs_local_file_header, SDL_IO_SEEK_SET);
			read_local_file_header(inputIo, &entries[entry].local_file_header);
		}

		/* write files */
		for (int entry = 0; entry < central_dir_end.num_entries_total; entry++)
		{
			entries[entry].ofs_local_file_header = SDL_TellIO(outputIo);
			entries[entry].local_file_header.len_extra = 0;
			write_local_file_header(outputIo, &entries[entry].local_file_header);
		}

		/* write central dir */
		central_dir_end.ofs_directory = SDL_TellIO(outputIo);
		for (int entry = 0; entry < central_dir_end.num_entries_total; entry++)
		{
			entries[entry].len_extra = 0;
			entries[entry].len_comment = 0;
			write_central_dir_entry(outputIo, &entries[entry]);
		}
		central_dir_end.len_directory = SDL_TellIO(outputIo) - central_dir_end.ofs_directory;

		/* write central dir end */
		central_dir_end.len_comment = 0;
		write_central_dir_end(outputIo, &central_dir_end);

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
		if (central_dir_end.comment)
			SDL_free(central_dir_end.comment);

		if (entries)
		{
			for (int entry = 0; entry < central_dir_end.num_entries_total; entry++)
			{
				if (entries[entry].filename) SDL_free(entries[entry].filename);
				if (entries[entry].extra) SDL_free(entries[entry].extra);
				if (entries[entry].comment) SDL_free(entries[entry].comment);
				if (entries[entry].local_file_header.filename) SDL_free(entries[entry].local_file_header.filename);
				if (entries[entry].local_file_header.extra) SDL_free(entries[entry].local_file_header.extra);
				if (entries[entry].local_file_header.data) SDL_free(entries[entry].local_file_header.data);
			}

			SDL_free(entries);
		}

		if (inputIo) SDL_CloseIO(inputIo);
		if (outputIo) SDL_CloseIO(outputIo);
	}

	SDL_Quit();

	return 0;
}
