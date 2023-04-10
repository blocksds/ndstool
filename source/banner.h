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
	unsigned_short title[16][BANNER_TITLE_LENGTH];		// max. 3 lines. seperated by linefeed character

	unsigned char anim_tile_data[8][4][4][8][4];
	unsigned char anim_tile_palette[8][16];
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
void IconFromBMP();
void IconFromGRF();
