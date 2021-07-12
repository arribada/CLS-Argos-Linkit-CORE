#pragma once

#include <string>

class Binascii {
public:
	static std::string hexlify(std::string input) {
	    static const char hex_digits[] = "0123456789ABCDEF";

	    std::string output;
	    output.reserve(input.length() * 2);
	    for (unsigned char c : input)
	    {
	        output.push_back(hex_digits[c >> 4]);
	        output.push_back(hex_digits[c & 0xF]);
	    }
	    return output;
	}

	static std::string unhexlify(std::string buffer) {
	    unsigned int length = buffer.size();
	    unsigned int i = 0;
	    char high, low;

	    std::string output;
	    if (length % 2) return output;

	    output.reserve(buffer.size() / 2);

	    while (length)
	    {
	    	high = buffer[i++];
	    	high = high >= 'A' ? high - '7' : high - '0';
	    	low = buffer[i++];
	    	low = low >= 'A' ? low - '7' : low - '0';
	    	output.push_back((high << 4) | (low & 0xF));
	    	length -= 2;
	    }

	    return output;
	}
};
