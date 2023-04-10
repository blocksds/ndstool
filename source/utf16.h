#pragma once

#include "little.h"

/**
 * @brief Convert from the system locale to UTF-16.
 * @param in Input string.
 * @param out Output buffer.
 * @param out_len Output buffer length, in bytes.
 * @return True if successful.
 */
bool utf16_convert_from_system(const char *in, size_t in_len, unsigned_short *out, size_t out_len);

/**
 * @brief Convert from UTF-16 to the system locale.
 * @param in Input buffer.
 * @param out Output string.
 * @param out_len Output string length, in bytes.
 * @return True if successful.
 */
bool utf16_convert_to_system(unsigned_short *in, size_t in_len, char *out, size_t out_len);
