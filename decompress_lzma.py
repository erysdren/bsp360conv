import lzma
import struct

class DecompressLzma():
	def __init__(self, uncompressed_size, properties):
		self.uncompressed_size = uncompressed_size
		self.properties = properties

	def decode(self, data):
		real_data = struct.pack("<BBBBBQ", self.properties[0], self.properties[1], self.properties[2], self.properties[3], self.properties[4], self.uncompressed_size) + data
		return lzma.decompress(real_data)
