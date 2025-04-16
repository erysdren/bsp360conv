
#include <SDL3/SDL.h>

#include "decompress_lzma.h"
#include "utils.h"

#define BSP_MAGIC 0x50534256
#define BSP_VERSION 20
#define BSP_NUM_LUMPS 64

#define VPHYSICS_MAGIC 0x59485056
#define VPHYSICS_VERSION 0x100

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

typedef struct vec4 {
	float x;
	float y;
	float z;
	float w;
} vec4_t;

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

typedef struct areaportal {
	Uint16 portal_key;
	Uint16 other_area;
	Uint16 first_clip_vert;
	Uint16 num_clip_verts;
	Sint32 plane_num;
} areaportal_t;

typedef struct leaf {
	Sint32 contents;
	Sint16 cluster;
	Uint16 flags;
	Sint16 mins[3];
	Sint16 maxs[3];
	Uint16 first_leaf_face;
	Uint16 num_leaf_faces;
	Uint16 first_leaf_brush;
	Uint16 num_leaf_brushes;
	Sint16 leaf_water_id;
} leaf_t;

typedef struct face {
	Uint16 plane_num;
	Uint8 side;
	Uint8 on_node;
	Sint32 first_edge;
	Sint16 num_edges;
	Sint16 tex_info;
	Sint16 disp_info;
	Sint16 surface_fog_volume;
	Uint8 styles[4];
	Sint32 light_offset;
	float area;
	Sint32 lightmap_mins[2];
	Sint32 lightmap_maxs[2];
	Sint32 original_face;
	Uint16 num_primitives;
	Uint16 first_primitive;
	Uint32 smoothing_groups;
} face_t;

typedef struct primitive {
	Uint8 type;
	Uint16 first_index;
	Uint16 num_indices;
	Uint16 first_vert;
	Uint16 num_verts;
} primitive_t;

typedef struct leaf_water_data {
	float surface_z;
	float min_z;
	Sint16 tex_info;
} leaf_water_data_t;

typedef struct phys_model {
	Sint32 model_index;
	Sint32 len_data;
	Sint32 len_key_data;
	Sint32 num_solids;
} phys_model_t;

typedef struct phys_solid {
	Sint32 id;
	Sint16 version;
	Sint16 type;
} phys_solid_t;

typedef struct phys_surface {
	Sint32 surface_size;
	vector_t axis;
	Sint32 axis_size;
} phys_surface_t;

typedef struct phys_compact_surface {
	vector_t mass_center;
	vector_t rotation_inertia;
	float upper_limit_radius;
	Uint32 bitfields;
	Sint32 ofs_ledgetree_root;
} phys_compact_surface_t;

typedef struct phys_compact_ledgetree_node {
	Sint32 ofs_right_node;
	Sint32 ofs_compact_ledge;
	vector_t center;
	float radius;
	Uint8 box_sizes[3];
	Uint8 padding;
} phys_compact_ledgetree_node_t;

typedef struct phys_compact_ledge {
	Sint32 ofs_point_array;
	Sint32 ofs_ledgetree_node;
	Uint32 bitfields;
	Sint16 num_triangles;
	Sint16 reserved;
} phys_compact_ledge_t;

SDL_COMPILE_TIME_ASSERT(phys_compact_ledge_size, sizeof(phys_compact_ledge_t) == 16);

typedef struct phys_compact_edge {
	Uint32 start_point_index : 16;
	Sint32 opposite_index : 15;
	Uint32 is_virtual : 1;
} phys_compact_edge_t;

SDL_COMPILE_TIME_ASSERT(phys_compact_edge_size, sizeof(phys_compact_edge_t) == 4);

typedef struct phys_compact_triangle {
	Uint32 bitfields;
	phys_compact_edge_t edges[3];
} phys_compact_triangle_t;

SDL_COMPILE_TIME_ASSERT(phys_compact_triangle_size, sizeof(phys_compact_triangle_t) == 16);

typedef struct overlay {
	Sint32 id;
	Sint16 tex_info;
	Uint16 num_faces;
	Sint32 faces[64];
	float u[2];
	float v[2];
	vector_t points[4];
	vector_t origin;
	vector_t normal;
} overlay_t;

typedef struct occluder_data {
	Sint32 flags;
	Sint32 first_poly;
	Sint32 num_polys;
	vector_t mins;
	vector_t maxs;
	Sint32 area;
} occluder_data_t;

typedef struct occluder_poly_data {
	Sint32 first_vert;
	Sint32 num_verts;
	Sint32 plane_num;
} occluder_poly_data_t;

typedef struct disp_edge_neighbor {
	Uint16 neighbor_index;
	Uint8 neighbor_orientation;
	Uint8 span;
	Uint8 neighbor_span;
} disp_edge_neighbor_t;

typedef struct disp_corner_neighbor {
	Uint16 neighbors[4];
	Uint8 num_neighbors;
} disp_corner_neighbor_t;

typedef struct disp_info {
	vector_t start_position;
	Sint32 first_vert;
	Sint32 first_tri;
	Sint32 power;
	Sint32 min_tess;
	float smoothing_angle;
	Sint32 contents;
	Uint16 map_face;
	Sint32 first_lightmap_alpha;
	Sint32 first_lightmap_sample_position;
	disp_edge_neighbor_t edge_neighbors[4][2];
	disp_corner_neighbor_t corner_neighbors[4];
	Uint32 allowed_verts[10];
} disp_info_t;

SDL_COMPILE_TIME_ASSERT(disp_info_size, sizeof(disp_info_t) == 176);

#define CHECK_FUNNY_LUMP_SIZE(s) if (lump_size % s != 0) return false;
#define SWAP16(x) x = SDL_Swap16(x)
#define SWAP32(x) x = SDL_Swap32(x)
#define SWAPFLOAT(x) x = SDL_SwapFloat(x)
#define SWAPVECTOR(v) (SWAPFLOAT(v.x), SWAPFLOAT(v.y), SWAPFLOAT(v.z))
#define SWAPVEC4(v) (SWAPFLOAT(v.x), SWAPFLOAT(v.y), SWAPFLOAT(v.z), SWAPFLOAT(v.w))

static void swap_compact_edge(phys_compact_edge_t *edge)
{
	Uint32 *bitfields = (Uint32 *)edge;

	Uint32 bitfield0 = (*bitfields & 0x0000FFFF) << 16;
	Uint32 bitfield1 = (*bitfields & 0x7FFF0000) >> 15;
	Uint32 bitfield2 = (*bitfields & 0x80000000) >> 31;

	*bitfields = bitfield0 | bitfield1 | bitfield2;

	SWAP32(*bitfields);
}

static void swap_compact_triangle(phys_compact_triangle_t *triangle)
{
	Uint32 bitfield0 = (triangle->bitfields & 0x00000FFF) << 20;
	Uint32 bitfield1 = (triangle->bitfields & 0x00FFF000) >> 4;
	Uint32 bitfield2 = (triangle->bitfields & 0x7F000000) >> 23;
	Uint32 bitfield3 = (triangle->bitfields & 0x80000000) >> 31;

	triangle->bitfields = bitfield0 | bitfield1 | bitfield2 | bitfield3;

	SWAP32(triangle->bitfields);

	for (int i = 0; i < 3; i++)
		swap_compact_edge(&triangle->edges[i]);
}

static void swap_compact_ledge(phys_compact_ledge_t *ledge)
{
	SWAP32(ledge->ofs_point_array);
	SWAP32(ledge->ofs_ledgetree_node);

	Uint32 bitfield0 = ledge->bitfields << 24;
	Uint32 bitfield00 = bitfield0 & 0x03000000;
	Uint32 bitfield01 = bitfield0 & 0x0C000000;
	Uint32 bitfield02 = bitfield0 & 0xF0000000;

	ledge->bitfields = ((bitfield00 << 6) | (bitfield01 << 2) | (bitfield02 >> 4)) | (ledge->bitfields >> 8);

	SWAP32(ledge->bitfields);

	SWAP16(ledge->num_triangles);
	SWAP16(ledge->reserved);

	/* swap triangles and points */
	/* TODO: how big should this be?? */
	bool *swapped_points = SDL_calloc(1024, sizeof(bool));

	vec4_t *points = (vec4_t *)((Uint8 *)ledge + ledge->ofs_point_array);
	phys_compact_triangle_t *triangles = (phys_compact_triangle_t *)(ledge + 1);
	for (int i = 0; i < ledge->num_triangles; i++)
	{
		swap_compact_triangle(&triangles[i]);

		/* swap points */
		Uint32 p0, p1, p2;
		p0 = triangles[i].edges[0].start_point_index;
		p1 = triangles[i].edges[1].start_point_index;
		p2 = triangles[i].edges[2].start_point_index;

#define PROCESS_POINT(n) if (swapped_points[n] == false) { SWAPVEC4(points[n]); swapped_points[n] = true; }
		PROCESS_POINT(p0);
		PROCESS_POINT(p1);
		PROCESS_POINT(p2);
#undef PROCESS_POINT

		/*
		log_info("%d: %0.4f %0.4f %0.4f %0.4f", p0, points[p0].x, points[p0].y, points[p0].z, points[p0].w);
		log_info("%d: %0.4f %0.4f %0.4f %0.4f", p1, points[p1].x, points[p1].y, points[p1].z, points[p1].w);
		log_info("%d: %0.4f %0.4f %0.4f %0.4f", p2, points[p2].x, points[p2].y, points[p2].z, points[p2].w);
		*/
	}

	SDL_free(swapped_points);
}

static void swap_ledgetree_node(phys_compact_ledgetree_node_t *ltn)
{
	SWAP32(ltn->ofs_right_node);
	SWAP32(ltn->ofs_compact_ledge);
	SWAPVECTOR(ltn->center);
	SWAPFLOAT(ltn->radius);

	/* has children */
	if (ltn->ofs_right_node != 0)
	{
		/* left child */
		phys_compact_ledgetree_node_t *left = ltn + 1;
		swap_ledgetree_node(left);

		/* right child */
		phys_compact_ledgetree_node_t *right = (phys_compact_ledgetree_node_t *)((Uint8 *)ltn + ltn->ofs_right_node);
		swap_ledgetree_node(right);
	}
}

static bool swap_lump(int lump, int lump_version, void *lump_data, Sint64 lump_size)
{
	switch (lump)
	{
		/* byte-sized data */
		case 0: /* entities */
		case 8: /* ldr lighting samples */
		case 34: /* displacement lightmap sample positions */
		case 43: /* texdata string data */
		case 53: /* hdr lighting samples */
		case 55: /* hdr ambient lighting samples  */
		case 56: /* ldr ambient lighting samples */
		{
			return true;
		}

		/* short-sized data */
		case 11: /* face ids */
		case 12: /* edges */
		case 16: /* leaf faces */
		case 17: /* leaf brushes */
		case 19: /* brush sides */
		case 31: /* vertex normal indices */
		case 39: /* primitive vertex indices */
		case 46: /* leaf distances to water */
		case 47: /* face macro texture info */
		case 48: /* displacement triangles */
		case 51: /* index of hdr lighting samples */
		case 52: /* index of ldr lighting samples */
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(Uint16));

			Uint16 *values = (Uint16 *)lump_data;
			for (int i = 0; i < lump_size / sizeof(Uint16); i++)
				SWAP16(values[i]);

			return true;
		}

		/* int-sized data */
		case 1: /* planes */
		case 2: /* texdata */
		case 3: /* vertices */
		case 6: /* texinfos */
		case 13: /* surfedges */
		case 14: /* models */
		case 15: /* ldr world lights */
		case 18: /* brushes */
		case 20: /* areas */
		case 30: /* vertex normals */
		case 33: /* displacement vertices */
		case 38: /* primitive vertices */
		case 41: /* clip portal vertices */
		case 42: /* cubemaps */
		case 44: /* texdata string table */
		case 54: /* hdr world lights */
		case 59: /* map flags */
		case 60: /* overlay fade distances */
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(Uint32));

			Uint32 *values = (Uint32 *)lump_data;
			for (int i = 0; i < lump_size / sizeof(Uint32); i++)
				SWAP32(values[i]);

			return true;
		}

		/* visibility */
		case 4:
		{
			Uint32 *vis = (Uint32 *)lump_data;
			SWAP32(vis[0]);
			for (int i = 0; i < vis[0] * 2; i++)
				SWAP32(vis[i + 1]);
			return true;
		}

		/* nodes */
		case 5:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(node_t));

			node_t *nodes = (node_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(node_t); i++)
			{
				SWAP32(nodes[i].plane_num);
				SWAP32(nodes[i].children[0]);
				SWAP32(nodes[i].children[1]);
				SWAP16(nodes[i].mins[0]);
				SWAP16(nodes[i].mins[1]);
				SWAP16(nodes[i].mins[2]);
				SWAP16(nodes[i].maxs[0]);
				SWAP16(nodes[i].maxs[1]);
				SWAP16(nodes[i].maxs[2]);
				SWAP16(nodes[i].first_face);
				SWAP16(nodes[i].num_faces);
				SWAP16(nodes[i].area);
				SWAP16(nodes[i].pad);
			}

			return true;
		}

		/* occlusion lump */
		case 9:
		{
			Uint8 *ptr = (Uint8 *)lump_data;
			Uint32 *count = (Uint32 *)ptr;
			SWAP32(*count);
			ptr += 4;

			for (int i = 0; i < *count; i++)
			{
				occluder_data_t *occluder_data = (occluder_data_t *)ptr;

				SWAP32(occluder_data[0].flags);
				SWAP32(occluder_data[0].first_poly);
				SWAP32(occluder_data[0].num_polys);

				SWAPVECTOR(occluder_data[0].mins);
				SWAPVECTOR(occluder_data[0].maxs);

				if (lump_version >= 1)
				{
					SWAP32(occluder_data[0].area);
					ptr += 40;
				}
				else
				{
					ptr += 36;
				}
			}

			count = (Uint32 *)ptr;
			SWAP32(*count);
			ptr += 4;

			occluder_poly_data_t *occluder_poly_data = (occluder_poly_data_t *)ptr;

			for (int i = 0; i < *count; i++)
			{
				SWAP32(occluder_poly_data[i].first_vert);
				SWAP32(occluder_poly_data[i].num_verts);
				SWAP32(occluder_poly_data[i].plane_num);
			}

			count = (Uint32 *)ptr;
			SWAP32(*count);
			ptr += 4;

			Uint32 *vertex_indices = (Uint32 *)ptr;

			for (int i = 0; i < *count; i++)
			{
				SWAP32(vertex_indices[i]);
			}

			return true;
		}

		/* leafs */
		case 10:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(leaf_t));

			leaf_t *leafs = (leaf_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(leaf_t); i++)
			{
				SWAP32(leafs[i].contents);
				SWAP16(leafs[i].cluster);
				SWAP16(leafs[i].flags);
				SWAP16(leafs[i].mins[0]);
				SWAP16(leafs[i].mins[1]);
				SWAP16(leafs[i].mins[2]);
				SWAP16(leafs[i].maxs[0]);
				SWAP16(leafs[i].maxs[1]);
				SWAP16(leafs[i].maxs[2]);
				SWAP16(leafs[i].first_leaf_face);
				SWAP16(leafs[i].num_leaf_faces);
				SWAP16(leafs[i].first_leaf_brush);
				SWAP16(leafs[i].num_leaf_brushes);
				SWAP16(leafs[i].leaf_water_id);
			}

			return true;
		}

		/* faces (ldr and hdr) */
		case 7:
		case 27:
		case 58:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(face_t));

			face_t *faces = (face_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(face_t); i++)
			{
				SWAP16(faces[i].plane_num);
				SWAP32(faces[i].first_edge);
				SWAP16(faces[i].num_edges);
				SWAP16(faces[i].tex_info);
				SWAP16(faces[i].disp_info);
				SWAP16(faces[i].surface_fog_volume);
				SWAP32(faces[i].light_offset);
				SWAPFLOAT(faces[i].area);
				SWAP32(faces[i].lightmap_mins[0]);
				SWAP32(faces[i].lightmap_mins[1]);
				SWAP32(faces[i].lightmap_maxs[0]);
				SWAP32(faces[i].lightmap_maxs[1]);
				SWAP32(faces[i].original_face);
				SWAP16(faces[i].num_primitives);
				SWAP16(faces[i].first_primitive);
				SWAP32(faces[i].smoothing_groups);
			}

			return true;
		}

		/* areaportals */
		case 21:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(areaportal_t));

			areaportal_t *areaportals = (areaportal_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(areaportal_t); i++)
			{
				SWAP16(areaportals[i].portal_key);
				SWAP16(areaportals[i].other_area);
				SWAP16(areaportals[i].first_clip_vert);
				SWAP16(areaportals[i].num_clip_verts);
				SWAP32(areaportals[i].plane_num);
			}

			return true;
		}

		/* disp info */
		case 26:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(disp_info_t));

			disp_info_t *disp_info = (disp_info_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(disp_info_t); i++)
			{
				SWAPVECTOR(disp_info[i].start_position);
				SWAP32(disp_info[i].first_vert);
				SWAP32(disp_info[i].first_tri);
				SWAP32(disp_info[i].power);
				SWAP32(disp_info[i].min_tess);
				SWAPFLOAT(disp_info[i].smoothing_angle);
				SWAP32(disp_info[i].contents);
				SWAP16(disp_info[i].map_face);
				SWAP32(disp_info[i].first_lightmap_alpha);
				SWAP32(disp_info[i].first_lightmap_sample_position);

				for (int j = 0; j < 4; j++)
				{
					for (int k = 0; k < 2; k++)
					{
						SWAP16(disp_info[i].edge_neighbors[j][k].neighbor_index);
					}
				}

				for (int j = 0; j < 4; j++)
				{
					for (int k = 0; k < 4; k++)
					{
						SWAP16(disp_info[i].corner_neighbors[j].neighbors[k]);
					}
				}

				for (int j = 0; j < 10; j++)
				{
					SWAP32(disp_info[i].allowed_verts[j]);
				}
			}

			return true;
		}

		/* phys disp */
		case 28:
		{
			Uint16 *ptr = (Uint16 *)lump_data;
			SWAP16(ptr[0]);
			for (int i = 0; i < ptr[0]; i++)
				SWAP16(ptr[i + 1]);
			return true;
		}

		/* phys models */
		case 29:
		{
			Uint8 *ptr = (Uint8 *)lump_data;

			while (1)
			{
				phys_model_t *header = (phys_model_t *)ptr;

				SWAP32(header->model_index);
				SWAP32(header->len_data);
				SWAP32(header->len_key_data);
				SWAP32(header->num_solids);

				if (header->model_index < 0 || header->len_data < 0)
					break;

				ptr += sizeof(phys_model_t);

				/* phy data */
				for (int i = 0; i < header->num_solids; i++)
				{
					SWAP32(*(Uint32 *)ptr);
					Uint32 size = *(Uint32 *)ptr;
					ptr += 4;

					phys_solid_t *solid = (phys_solid_t *)ptr;

					SWAP32(solid->id);
					SWAP16(solid->version);
					SWAP16(solid->type);

					/* sanity check */
					if (solid->id != VPHYSICS_MAGIC)
					{
						log_warning("solid %d has incorrect magic value 0x%08x (should be 0x%08x)", i, solid->id, VPHYSICS_MAGIC);
						continue;
					}

					/* sanity check */
					if (solid->version != VPHYSICS_VERSION)
					{
						log_warning("solid %d has incorrect version value 0x%04x (should be 0x%04x)", i, solid->version, VPHYSICS_VERSION);
						continue;
					}

					if (solid->type == 0) /* poly */
					{
						/* swap nasty ivp shit */
						phys_surface_t *surface = (phys_surface_t *)(ptr + sizeof(phys_solid_t));

						SWAP32(surface->surface_size);
						SWAPVECTOR(surface->axis);
						SWAP32(surface->axis_size);

						phys_compact_surface_t *compact_surface = (phys_compact_surface_t *)(surface + 1);

						SWAPVECTOR(compact_surface->mass_center);
						SWAPVECTOR(compact_surface->rotation_inertia);
						SWAPFLOAT(compact_surface->upper_limit_radius);

						Uint8 max_factor_surface_deviation = compact_surface->bitfields & 0xFF;
						Uint32 byte_size = (compact_surface->bitfields & 0xFFFFFF00);

						SWAP32(byte_size);

						compact_surface->bitfields = byte_size << 8 | max_factor_surface_deviation;

						SWAP32(compact_surface->ofs_ledgetree_root);

						/* sanity check */
						if (byte_size != surface->surface_size)
						{
							log_warning("solid %d: size mismatch", i);
							return false;
						}

						/* get ledgetree node root */
						phys_compact_ledgetree_node_t *ltn = (phys_compact_ledgetree_node_t *)((Uint8 *)compact_surface + compact_surface->ofs_ledgetree_root);

						/* recurse tree */
						swap_ledgetree_node(ltn);

						/* has compact ledge */
						if (ltn->ofs_compact_ledge != 0)
						{
							phys_compact_ledge_t *ledge = (phys_compact_ledge_t *)((Uint8 *)ltn + ltn->ofs_compact_ledge);
							swap_compact_ledge(ledge);
						}
					}
					else if (solid->type == 1) /* mopp */
					{
						log_warning("solid %d: COLLIDE_MOPP unsupported", i);
						return false;
					}
					else if (solid->type == 2) /* ball */
					{
						log_warning("solid %d: COLLIDE_BALL unsupported", i);
						return false;
					}
					else if (solid->type == 3) /* virtual */
					{
						log_warning("solid %d: COLLIDE_VIRTUAL unsupported", i);
						return false;
					}
					else /* unknown */
					{
						log_warning("solid %d: unknown type %d", i, solid->type);
						return false;
					}

					ptr += size;
				}

				/* text data */
				ptr += header->len_key_data;
			}

			return true;
		}

		/* game lumps */
		case 35:
		{
			/* HACKHACK */
			Uint32 *temp = (Uint32 *)lump_data;
			*temp = 0;
			return true;
		}

		/* leaf water data */
		case 36:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(leaf_water_data_t));

			leaf_water_data_t *leaf_water_datas = (leaf_water_data_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(leaf_water_data_t); i++)
			{
				SWAPFLOAT(leaf_water_datas[i].surface_z);
				SWAPFLOAT(leaf_water_datas[i].min_z);
				SWAP16(leaf_water_datas[i].tex_info);
			}

			return true;
		}

		/* primtiives */
		case 37:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(primitive_t));

			primitive_t *primitives = (primitive_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(primitive_t); i++)
			{
				SWAP16(primitives[i].first_index);
				SWAP16(primitives[i].num_indices);
				SWAP16(primitives[i].first_vert);
				SWAP16(primitives[i].num_verts);
			}

			return true;
		}

		/* pakfile */
		case 40:
		{
			/* TODO */
			return false;
		}

		/* overlays */
		case 45:
		{
			CHECK_FUNNY_LUMP_SIZE(sizeof(overlay_t));

			overlay_t *overlays = (overlay_t *)lump_data;
			for (int i = 0; i < lump_size / sizeof(overlay_t); i++)
			{
				SWAP32(overlays[i].id);
				SWAP16(overlays[i].tex_info);
				SWAP16(overlays[i].num_faces);
				for (int j = 0; j < 64; j++)
					SWAP32(overlays[i].faces[j]);
				SWAPFLOAT(overlays[i].u[0]);
				SWAPFLOAT(overlays[i].u[1]);
				SWAPFLOAT(overlays[i].v[0]);
				SWAPFLOAT(overlays[i].v[1]);
				for (int j = 0; j < 4; j++)
					SWAPVECTOR(overlays[i].points[j]);
				SWAPVECTOR(overlays[i].origin);
				SWAPVECTOR(overlays[i].normal);
			}

			return true;
		}

		/* unknown lump */
		default:
		{
			return false;
		}
	}
}

#undef SWAPVECTOR
#undef SWAPFLOAT
#undef SWAP32
#undef SWAP16
#undef CHECK_FUNNY_LUMP_SIZE

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
					log_warning("Lump %d: Uncompressed size mismatch %d != %d", lump, inputHeader.lumps[lump].identifier, uncompressed_size);
					goto cleanup;
				}

				/* byteswap data */
				if (!swap_lump(lump, inputHeader.lumps[lump].version, uncompressed, uncompressed_size))
				{
					log_warning("Lump %d: Failed to byteswap data", lump);
					inputHeader.lumps[lump].offset = 0;
					inputHeader.lumps[lump].length = 0;
				}
				else
				{
					/* save new offset and size */
					inputHeader.lumps[lump].offset = SDL_TellIO(outputIo);
					inputHeader.lumps[lump].length = uncompressed_size;

					/* write lump data */
					SDL_WriteIO(outputIo, uncompressed, uncompressed_size);
				}

				/* clean up */
				SDL_free(uncompressed);
			}
			else if (inputHeader.lumps[lump].length > 0)
			{
				/* read lump data */
				void *lump_data = SDL_malloc(inputHeader.lumps[lump].length);
				SDL_ReadIO(inputIo, lump_data, inputHeader.lumps[lump].length);

				/* byteswap data */
				if (!swap_lump(lump, inputHeader.lumps[lump].version, lump_data, inputHeader.lumps[lump].length))
				{
					log_warning("Lump %d: Failed to byteswap data", lump);
					inputHeader.lumps[lump].offset = 0;
					inputHeader.lumps[lump].length = 0;
				}
				else
				{
					/* save new offset */
					inputHeader.lumps[lump].offset = SDL_TellIO(outputIo);

					/* write lump data */
					SDL_WriteIO(outputIo, lump_data, inputHeader.lumps[lump].length);
				}

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
