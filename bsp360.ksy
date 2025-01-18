meta:
  id: bsp360
  title: Source Engine BSP (Xbox 360)
  application: Source Engine
  file-extension: bsp
  license: CC0-1.0
  endian: be

doc: |
  This specification was authored by erysdren (it/its).
  https://erysdren.me/

seq:
  - id: magic
    contents: "PSBV"
  - id: version
    type: s4
  - id: lumps
    type: lump(_index)
    repeat: expr
    repeat-expr: num_lumps
  - id: map_version
    type: s4
  - id: unknown
    type: s4

instances:
  num_lumps:
    value: 64

types:
  vec3f:
    seq:
      - id: x
        type: f4
      - id: y
        type: f4
      - id: z
        type: f4

  plane:
    seq:
      - id: normal
        type: vec3f
      - id: dist
        type: f4
      - id: type
        type: s4

  lzma:
    seq:
      - id: magic
        contents: "LZMA"
      - id: uncompressed_size
        type: u4le
      - id: compressed_size
        type: u4le
      - id: properties
        type: u1
        repeat: expr
        repeat-expr: 5
      - id: data
        size: compressed_size
        process: decompress_lzma(uncompressed_size, properties)

  game_lump:
    seq:
      - id: identifier
        type: str
        encoding: ascii
        size: 4
      - id: flags
        type: u2
      - id: version
        type: u2
      - id: ofs_data
        type: s4
      - id: len_data
        type: s4
    instances:
      data:
        pos: ofs_data
        size: len_data
        if: len_data > 0
        io: _root._io
      lzma_test:
        pos: ofs_data
        type: str
        encoding: ascii
        size: 4
        if: len_data > 0
        io: _root._io
      is_lzma:
        value: lzma_test == "LZMA"
        if: len_data > 0
      lzma:
        pos: ofs_data
        size: len_data
        if: len_data > 0 and is_lzma
        type: lzma
        io: _root._io

  game_lumps:
    seq:
      - id: num_lumps
        type: s4
      - id: lumps
        type: game_lump
        repeat: expr
        repeat-expr: num_lumps

  lump:
    params:
      - id: index
        type: s4
    seq:
      - id: ofs_data
        type: s4
      - id: len_data
        type: s4
      - id: version
        type: s4
      - id: identifier
        size: 4
    instances:
      data:
        pos: ofs_data
        size: len_data
        if: len_data > 0
      lzma_test:
        pos: ofs_data
        type: str
        encoding: ascii
        size: 4
        if: len_data > 0
      is_lzma:
        value: lzma_test == "LZMA"
        if: len_data > 0
      lzma:
        pos: ofs_data
        size: len_data
        if: len_data > 0 and is_lzma
        type: lzma
      game_lumps:
        pos: ofs_data
        size: len_data
        type: game_lumps
        if: index == 35

enums:
  lump:
    0: entities
    1: planes
    2: tex_data
    3: vertices
    4: visibility
    5: nodes
    6: tex_info
    7: faces
    8: lighting
    9: occlusion
    10: leafs
    11: face_ids
    12: edges
    13: surf_edges
    14: models
    15: world_lights
    16: leaf_faces
    17: leaf_brushes
    18: brushes
    19: brush_sides
    20: areas
    21: areaportals
    22: unused0
    23: unused1
    24: unused2
    25: unused3
    26: disp_info
    27: original_faces
    28: phys_disp
    29: phys_collide
    30: vertex_normals
    31: vertex_normal_indices
    32: disp_lighmtap_alphas
    33: disp_verts
    34: disp_lightmap_sample_positions
    35: game
    36: leaf_water_data
    37: primitives
    38: primitive_vertices
    39: primitive_indices
    40: pakfile
    41: clip_portal_verts
    42: cubemaps
    43: tex_data_string_data
    44: tex_data_string_table
    45: overlays
    46: leaf_min_dist_to_water
    47: face_macro_texture_info
    48: disp_tris
    49: phys_collide_surface
    50: water_overlays
    51: leaf_ambient_index_hdr
    52: leaf_ambient_index
    53: lighting_hdr
    54: world_lights_hdr
    55: leaf_ambient_lighting_hdr
    56: leaf_ambient_lighting
    57: xzippakfile
    58: faces_hdr
    59: map_flags
    60: overlay_fades
