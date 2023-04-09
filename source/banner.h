#define BANNER_TITLE_LENGTH 128

#pragma pack(1)

struct Banner
{
	unsigned_short version;
	unsigned_short crc;
	unsigned char reserved[28];
	unsigned char tile_data[4][4][8][4];
	unsigned_short palette[16];
	unsigned_short title[16][BANNER_TITLE_LENGTH];		// max. 3 lines. seperated by linefeed character

	unsigned char anim_tile_data[8][4][4][8][4];
	unsigned char anim_tile_palette[8][16];
	unsigned_short anim_sequence[64];
};

#pragma pack()

extern const char *bannerLanguages[];

static inline int GetBannerLanguageCount(unsigned short version)
{
	switch (version)
	{
		default:
		case 0x0001: return 6;
		case 0x0002: return 7;
		case 0x0003:
		case 0x0103: return 8;
	}
}

static inline unsigned int CalcBannerSize(unsigned short version)
{
	switch (version)
	{
		default:     version = 1; /* fallthrough */
		case 0x0001:
		case 0x0002:
		case 0x0003: return 0x840 + 0x100 * (version - 1);
		case 0x0103: return 0x23C0;
	}
};

unsigned short CalcBannerCRC(Banner &banner);
void IconFromBMP();
void IconFromGRF();
