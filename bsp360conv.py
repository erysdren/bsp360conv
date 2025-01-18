#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import struct
from bsp360 import Bsp360

bsp360 = Bsp360.from_file("xbla_1.360.bsp")

bsp = open("xbla_1.bsp", "wb")

bsp.write(bytes("VBSP".encode("ascii")))
bsp.write(struct.pack("<L", 20))

# write initial lump headers
for lump in bsp360.lumps:
	bsp.write(struct.pack("<L", 0)) # offset
	bsp.write(struct.pack("<L", 0)) # size
	bsp.write(struct.pack("<l", lump.version))
	bsp.write(struct.pack("<L", 0))
	# bsp.write(lump.identifier)

# write other header info
bsp.write(struct.pack("<L", bsp360.map_version))
bsp.write(struct.pack("<L", bsp360.unknown))

# write lump data
lumps = []
for i, lump in enumerate(bsp360.lumps):
	if lump.len_data <= 0:
		lumps.append((0, 0))
	elif lump.is_lzma:
		lumps.append((bsp.tell(), lump.lzma.uncompressed_size))
		bsp.write(lump.lzma.data)
	elif i == 35:
		lumps.append((bsp.tell(), 0))
		game_lumps = []
		bsp.write(struct.pack("<l", lump.game_lumps.num_lumps))
		for game_lump in lump.game_lumps.lumps:
			bsp.write(bytes(game_lump.identifier.encode("ascii")))
			bsp.write(struct.pack("<H", game_lump.flags))
			bsp.write(struct.pack("<H", game_lump.version))
			bsp.write(struct.pack("<L", 0))
			bsp.write(struct.pack("<L", 0))
		for g, game_lump in enumerate(lump.game_lumps.lumps):
			if game_lump.len_data <= 0:
				game_lumps.append((0, 0))
			elif game_lump.is_lzma:
				game_lumps.append((bsp.tell(), game_lump.lzma.uncompressed_size))
				bsp.write(game_lump.lzma.data)
			else:
				game_lumps.append((bsp.tell(), game_lump.len_data))
				bsp.write(game_lump.data)
		lumps[i] = (lumps[i][0], bsp.tell() - lumps[i][0])
		bsp.seek(lumps[i][0])
		bsp.write(struct.pack("<l", lump.game_lumps.num_lumps))
		for g, game_lump in enumerate(lump.game_lumps.lumps):
			bsp.write(bytes(game_lump.identifier.encode("ascii")))
			bsp.write(struct.pack("<H", game_lump.flags))
			bsp.write(struct.pack("<H", game_lump.version))
			bsp.write(struct.pack("<L", game_lumps[g][0]))
			bsp.write(struct.pack("<L", game_lumps[g][1]))
	else:
		lumps.append((bsp.tell(), lump.len_data))
		bsp.write(lump.data)

# rewrite lump info
bsp.seek(8)
for i, lump in enumerate(bsp360.lumps):
	bsp.write(struct.pack("<L", lumps[i][0])) # offset
	bsp.write(struct.pack("<L", lumps[i][1])) # size
	bsp.write(struct.pack("<l", lump.version))
	bsp.write(struct.pack("<L", 0))
	# bsp.write(lump.identifier)

bsp.close()
