#include <cstring>
#include <iconv.h>
#include <locale.h>
#include "utf16.h"

static bool iconv_utf16_initialized = false;
static iconv_t iconv_utf16_from_system;
static iconv_t iconv_utf16_to_system;

static size_t utf16_wstrlen(unsigned_short *in) {
	int i = 0;
	while (in[i] != 0) i++;
	return i;
}

static void utf16_iconv_init(void) {
	if (iconv_utf16_initialized) {
		return;
	}

	setlocale(LC_ALL, "");
	iconv_utf16_to_system = iconv_open("", "UTF-16LE");
	iconv_utf16_from_system = iconv_open("UTF-16LE", "");

	iconv_utf16_initialized = true;
}

bool utf16_convert_from_system(const char *in, size_t in_len, unsigned_short *out, size_t out_len) {
	utf16_iconv_init();
	if (in_len == 0) in_len = strlen(in) + 1;

	return iconv(iconv_utf16_from_system, (char**)&in, &in_len, (char**)&out, &out_len) != (size_t) -1;
}

bool utf16_convert_to_system(unsigned_short *in, size_t in_len, char *out, size_t out_len) {
	utf16_iconv_init();
	if (in_len == 0) in_len = (utf16_wstrlen(in) + 1) * 2;

	return iconv(iconv_utf16_to_system, (char**)&in, &in_len, (char**)&out, &out_len) != (size_t) -1;
}
