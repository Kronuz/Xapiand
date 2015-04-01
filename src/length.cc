#include "length.h"

std::string encode_length(size_t len)
{
    std::string result;
    if (len < 255) {
	result += static_cast<unsigned char>(len);
    } else {
	result += '\xff';
	len -= 255;
	while (true) {
	    unsigned char b = static_cast<unsigned char>(len & 0x7f);
	    len >>= 7;
	    if (!len) {
		result += (b | static_cast<unsigned char>(0x80));
		break;
	    }
	    result += b;
	}
    }
    return result;
}

size_t
decode_length(const char ** p, const char *end, bool check_remaining)
{
    if (*p == end) {
	return -1;
    }

    size_t len = static_cast<unsigned char>(*(*p)++);
    if (len == 0xff) {
	len = 0;
	unsigned char ch;
	int shift = 0;
	do {
	    if (*p == end || shift > 28)
		return -1;
	    ch = *(*p)++;
	    len |= size_t(ch & 0x7f) << shift;
	    shift += 7;
	} while ((ch & 0x80) == 0);
	len += 255;
    }
    if (check_remaining && len > size_t(end - *p)) {
	return -1;
    }
    return len;
}
