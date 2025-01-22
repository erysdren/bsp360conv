meta:
  id: xzip360
  title: Source Engine XZip (Xbox 360)
  application: Source Engine
  file-extension: zip
  license: CC0-1.0
  endian: be

doc: |
  This specification was authored by erysdren (it/its).
  https://erysdren.me/

seq:
  - id: sections
    type: section
    repeat: eos

types:
  section:
    seq:
      - id: magic
        contents: "PK"
      - id: type
        type: u2
      - id: body
        type:
          switch-on: type
          cases:
            0x0201: central_dir_entry
            0x0403: local_file
            0x0605: end_of_central_dir

  local_file:
    seq:
      - id: version
        type: u2
      - id: flags
        type: u2
      - id: compression_method
        type: u2
      - id: last_modified_time
        type: u2
      - id: last_modified_date
        type: u2
      - id: crc32
        type: u4
      - id: compressed_size
        type: u4le
      - id: uncompressed_size
        type: u4le
      - id: len_filename
        type: u2le
      - id: len_extras
        type: u2le
      - id: filename
        type: strz
        encoding: utf-8
        size: len_filename
      - id: extras
        size: len_extras
      - id: file_data
        size: compressed_size
        type:
          switch-on: filename
          cases:
            '__preload_section.pre': preload_section

  central_dir_entry:
    doc-ref: https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT - 4.3.12
    seq:
      - id: version_made_by
        type: u2
      - id: version_needed_to_extract
        type: u2
      - id: flags
        type: u2
      - id: compression_method
        type: u2
        enum: compression
      - id: last_mod_file_time
        type: u2
      - id: last_mod_file_date
        type: u2
      - id: crc32
        type: u4
      - id: compressed_size
        type: u4
      - id: uncompressed_size
        type: u4
      - id: file_name_len
        type: u2le
      - id: extra_len
        type: u2le
      - id: comment_len
        type: u2le
      - id: disk_number_start
        type: u2
      - id: int_file_attr
        type: u2
      - id: ext_file_attr
        type: u4
      - id: local_header_offset
        type: s4
      - id: file_name
        type: str
        size: file_name_len
        encoding: UTF-8
      - id: extra
        size: extra_len
        type: extras
      - id: comment
        type: str
        size: comment_len
        encoding: UTF-8
    instances:
      local_header:
        pos: local_header_offset
        type: section
