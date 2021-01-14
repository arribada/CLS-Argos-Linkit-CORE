#ifndef __BCH_HPP_
#define __BCH_HPP_

#include <vector>
#include <string_view>

using BCHCodeWord = unsigned int;

class BCHEncoder {

public:

	// 0x26D9E3 (0o11554743)
	static constexpr const unsigned char B127_106_3[] = {1,0, 0,1,1,0, 1,1,0,1, 1,0,0,1, 1,1,1,0, 0,0,1,1};
	static constexpr const unsigned int B127_106_3_CODE_LEN = sizeof(B127_106_3) - 1;

	// 0x1EE5B42FD (0o75626641375)
	static constexpr const unsigned char B255_223_4[] = {1, 1,1,1,0, 1,1,1,0, 0,1,0,1, 1,0,1,1, 0,1,0,0, 0,0,1,0, 1,1,1,1, 1,1,0,1};
	static constexpr const unsigned int B255_223_4_CODE_LEN = sizeof(B255_223_4) - 1;

	static BCHCodeWord encode(const unsigned char poly[], unsigned int poly_size, std::string const& data, unsigned int total_bits)
	{
		std::vector<unsigned char> msg_as_bits;
		std::vector<unsigned char> remainder;
		BCHCodeWord code_word = 0;

		for(unsigned int i = 0, byte_index = 0, bit_index = 0; i < total_bits; ++i)
		{
			if((data[byte_index] & (1 << (7 - bit_index))) == 0) {
				//std::cout << "i:" << i << " byte: " << byte_index << " bit: " << bit_index << " (0)" << std::endl;
				msg_as_bits.push_back(0);
			}
			else
			{
				//std::cout << "i:" << i << " byte: " << byte_index << " bit: " << bit_index << " (1)" << std::endl;
				msg_as_bits.push_back(1);
			}

			bit_index++;

			if (bit_index == 8)
			{
				bit_index = 0;
				byte_index++;
			}
		}

		//for (unsigned int i = 0; i < msg_as_bits.size(); i++)
		//	std::cout << (unsigned int)msg_as_bits[i];
		//std::cout << std::endl;

		unsigned int remainder_size = total_bits + poly_size - 1;

		// Copy the message into the top end of the remainder and extend with zeroes
		remainder = msg_as_bits;
		remainder.resize(remainder_size, 0);

		// Do finite field euclidian division
		for (unsigned int i = 0; i < total_bits; ++i)
		{
			if (remainder[i])
			{
				for (unsigned int j = 0; j < poly_size; ++j)
					remainder.at(i + j) ^= poly[j];
			}
		}

		// The result sits at the end of the remainder buffer
		for (unsigned int i = 0; i < remainder_size - total_bits; ++i)
			code_word |= (!!remainder[total_bits + i]) << (remainder_size - total_bits - i - 1);

		return code_word;
	}
};

#endif // __BCH_HPP_
