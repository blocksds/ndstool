#define BANNER_TITLE_LENGTH 128

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

void FixBannerCRC(char *ndsfilename);
int GetBannerLanguageCount(unsigned short version);
unsigned int CalcBannerSize(unsigned short version);
unsigned short GetBannerMinVersionForCRCSlot(unsigned short slot);
unsigned short CalcBannerCRC(Banner &banner, unsigned short slot);
void IconFromBMP();
void IconFromGRF();
