#ifndef __BIT_PACK_HPP_
#define __BIT_PACK_HPP_

#define EXTRACT_BITS(dest, source, start, num_bits)  do { dest = extract_bits(source, start, num_bits); start += num_bits; } while (0)
#define EXTRACT_BITS_CAST(type, dest, source, start, num_bits)  do { dest = (type)extract_bits(source, start, num_bits); start += num_bits; } while (0)

// Note: Function unpacks values MSB first (big endian) to LSB first (little endian)

static uint32_t extract_bits(const std::string& data, int start, int total_bits) {
	uint32_t result = 0;
	int num_bits = total_bits;
	int in_byte_offset = start / 8;
	int in_bit_offset = start % 8;
	int out_bit_offset = 0;
	int n_bits = std::min({8 - in_bit_offset, num_bits});

	while (num_bits) {
		int mask = 0xFF >> (8 - n_bits);
		result |= ((uint32_t)((data[in_byte_offset] >> (8-in_bit_offset-n_bits)) & mask) << (total_bits - out_bit_offset - n_bits));
		out_bit_offset = out_bit_offset + n_bits;
		in_bit_offset = in_bit_offset + n_bits;

		if (in_bit_offset >= 8) {
			in_byte_offset++;
			in_bit_offset %= 8;
		}

		num_bits -= n_bits;
		n_bits = std::min({8 - in_bit_offset, num_bits});
	}

	return result;
}

#define PACK_BITS(source, dest, start, num_bits)  do { pack_bits(dest, source, start, num_bits); start += num_bits; } while (0)
#define PACK_BITS_CAST(type, source, dest, start, num_bits)  do { pack_bits(dest, (type)source, start, num_bits); start += num_bits; } while (0)

// Note: Function packs values MSB first (big endian) from LSB first (little endian)

static void pack_bits(std::string& output, uint32_t value, int start, int total_bits) {

	int num_bits = total_bits;
	int out_byte_offset = start / 8;
	int out_bit_offset = start % 8;
	int in_bit_offset = 0;
	int n_bits = std::min({8 - out_bit_offset, num_bits});

	while (num_bits) {
		unsigned int mask = ((total_bits - in_bit_offset) == 32 ? 0xFFFFFFFF : ((1 << (total_bits - in_bit_offset))-1)) >> (total_bits - in_bit_offset - n_bits);
		output[out_byte_offset] |= (((value >> (total_bits - in_bit_offset - n_bits)) & mask) << (8-out_bit_offset-n_bits));
		out_bit_offset = out_bit_offset + n_bits;
		in_bit_offset = in_bit_offset + n_bits;

		if (out_bit_offset >= 8) {
			out_byte_offset++;
			out_bit_offset %= 8;
		}
		num_bits -= n_bits;
		n_bits = std::min({8 - out_bit_offset, num_bits});
	}
}

#endif  // __BIT_PACK_HPP_
