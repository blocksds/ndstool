// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#pragma once

#define MAX_BANNER_TITLE_COUNT 16
#define BANNER_TITLE_LENGTH 128
#define NUM_VERSION_CRCS 4
#define BAD_MIN_VERSION_CRC 0xFFFF

#pragma pack(1)

struct Banner
{
	unsigned_short version;
	unsigned_short crc[15];
	unsigned char tile_data[4][4][8][4];
	unsigned_short palette[16];
	// max. 3 lines. seperated by linefeed character
	unsigned_short title[MAX_BANNER_TITLE_COUNT][BANNER_TITLE_LENGTH];

	unsigned char anim_tile_data[8][4][4][8][4];
	unsigned_short anim_palette[8][16];
	unsigned_short anim_sequence[64];
};

#pragma pack()

extern const char *bannerLanguages[];

unsigned short ExtractBannerVersion(FILE *fNDS, unsigned int banner_offset);
void FixBannerCRC(char *ndsfilename, unsigned int banner_offset, unsigned int bannersize);
int GetBannerLanguageCount(unsigned short version);
unsigned int CalcBannerSize(unsigned short version);
unsigned short GetBannerMinVersionForCRCSlot(unsigned short slot);
unsigned short CalcBannerCRC(Banner &banner, unsigned short slot, unsigned int bannersize);
void IconToRasterImage();
void IconFromRasterImage();
void IconFromGRF();
