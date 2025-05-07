#include "crc.h"

UINT32 compute(unsigned char* buffer, size_t len) {
	UINT32 result = crcSeed;

	for (size_t i = 0; i < len; i++) {
		result = hashTable[((unsigned char)result) ^ ((unsigned char)buffer[i])] ^ (result >> 8);
	}

	return result;
}