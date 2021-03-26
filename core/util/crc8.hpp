#ifndef __CRC8_HPP_
#define __CRC8_HPP_

#include <algorithm>
#include <string>
#include "bitpack.hpp"

class CRC8 {
public:
	// Zero pad the start of the data buffer so the number of total bits becomes a multiple of 8
	static std::string zeropad(std::string const & data, unsigned int total_bits) {
		unsigned int remainder = (8 - (total_bits % 8)) % 8;
		std::string buffer;

		if (remainder == 0)
		{
			buffer = data.substr(0, total_bits / 8);
			return buffer;
		}
		else
		{
			buffer.assign((total_bits + remainder) / 8, 0);
			unsigned int op_base_pos = remainder;
			unsigned int ip_base_pos = 0;
			unsigned char byte;
			while (total_bits) {
				unsigned int bits = std::min(8U, total_bits);
				total_bits -= bits;
				EXTRACT_BITS(byte, data, ip_base_pos, bits);
				PACK_BITS(byte, buffer, op_base_pos, bits);
			}
		}

		return buffer;
	}

	static unsigned char checksum(std::string const & data, unsigned int total_bits) {
	  unsigned int crc = 0;
	  int j, idx;
	  std::string buffer = zeropad(data, total_bits);

	  for (idx = 0, j = buffer.size(); j; j--, idx++) {
	    crc ^= (buffer[idx] << 8);
	    for(int i = 8; i; i--) {
	      if (crc & 0x8000)
	        crc ^= (0x1070 << 3);
	      crc <<= 1;
	    }
	  }
	  return (unsigned char)(crc >> 8);
	}
};

#endif // __CRC8_HPP_
