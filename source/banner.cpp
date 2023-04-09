#include "ndstool.h"
#include "raster.h"
#include "banner.h"
#include "crc.h"
#include "utf16.h"

const char *bannerLanguages[] = { "Japanese", "English", "French", "German", "Italian", "Spanish", "Chinese", "Korean" };

#define RGB16(r,g,b)			((r) | (g<<5) | (b<<10))

/*
 * RGBQuadToRGB16
 */
inline unsigned short RGBQuadToRGB16(RGBQUAD quad)
{
	unsigned short r = quad.rgbRed;
	unsigned short g = quad.rgbGreen;
	unsigned short b = quad.rgbBlue;
	return RGB16(r>>3, g>>3, b>>3);
}

unsigned short GetBannerMinVersionForCRCSlot(unsigned short slot)
{
	switch(slot)
	{
		case 0: return 0x0001;
		case 1: return 0x0002;
		case 2: return 0x0003;
		case 3: return 0x0103;
		default: return 0xFFFF;
	}
}

unsigned short CalcBannerCRC(Banner &banner, unsigned short slot)
{
	if (banner.version < GetBannerMinVersionForCRCSlot(slot))
	{
		return 0;
	}

	switch(slot)
	{
		case 0: return CalcCrc16((unsigned char *)&banner + 0x20, CalcBannerSize(0x0001) - 0x20);
		case 1: return CalcCrc16((unsigned char *)&banner + 0x20, CalcBannerSize(0x0002) - 0x20);
		case 2: return CalcCrc16((unsigned char *)&banner + 0x20, CalcBannerSize(0x0003) - 0x20);
		case 3: return CalcCrc16((unsigned char *)&banner + 0x1240, CalcBannerSize(0x0103) - 0x1240);
		default: return 0;
	}
}

int GetBannerLanguageCount(unsigned short version)
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

unsigned int CalcBannerSize(unsigned short version)
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

void BannerPutTitle(const char *text, Banner &banner)
{
	// convert initial title
	if (!utf16_convert_from_system(text, 0, banner.title[0], BANNER_TITLE_LENGTH * 2))
	{
		fprintf(stderr, "WARNING: UTF-16 conversion failed, using fallback.\n");
		for (int i=0; bannertext[i] && (i<BANNER_TITLE_LENGTH); i++)
		{
			banner.title[0][i] = bannertext[i];
		}
	}
	banner.title[0][BANNER_TITLE_LENGTH-1] = 0;

	// convert ; to newline
	for (int i=0; banner.title[0][i]; i++)
	{
		if (banner.title[0][i] == ';')
			banner.title[0][i] = 0x0A;
	}

	// copy to other languages
	for (int l=1; l<GetBannerLanguageCount(banner.version); l++)
	{
		memcpy(banner.title[l], banner.title[0], sizeof(banner.title[0]));
	}
}

/*
 * IconFromBMP
 */
void IconFromBMP()
{
	CRaster bmp;
	int rval = bmp.LoadBMP(bannerfilename);
	if (rval < 0) exit(1);

	if (bmp.width != 32 || bmp.height != 32) {
		fprintf(stderr, "Image should be 32 x 32.\n");
		exit(1);
	}

	Banner banner;
	memset(&banner, 0, sizeof(banner));
	banner.version = 1;

	// tile data (4 bit / tile, 4x4 total tiles)
	// 32 bytes per tile (in 4 bit mode)
	for (int row=0; row<4; row++)
	{
		for (int col=0; col<4; col++)
		{
			for (int y=0; y<8; y++)
			{
				for (int x=0; x<8; x+=2)
				{
					unsigned char b0 = bmp[row*8 + y][col*8 + x + 0];
					unsigned char b1 = bmp[row*8 + y][col*8 + x + 1];
					banner.tile_data[row][col][y][x/2] = (b1 << 4) | b0;
				}
			}
		}
	}

	// palette
	for (int i = 0; i < 16; i++)
	{
		banner.palette[i] = RGBQuadToRGB16(bmp.palette[i]);
	}

	BannerPutTitle(bannertext, banner);
	for (int slot=0; slot<4; slot++)
	{
		banner.crc[slot] = CalcBannerCRC(banner, slot);
	}

	fwrite(&banner, 1, CalcBannerSize(banner.version), fNDS);
}

/*
 * IconFromGRF
 * 
 * Assumes Input File to be a 32x32 pixel 4bpp tiled image
 * grit command line:
 *      grit icon.png -g -gt -gB4 -gT <color> -m! -p -pe 16 -fh! -ftr
 */

typedef struct {
	unsigned char GfxAttr;
	unsigned char MapAttr;
	unsigned char MMapAttr;
	unsigned char PalAttr;
	
	unsigned char TileWidth;
	unsigned char TileHeight;
	unsigned char MetaTileWidth;
	unsigned char MetaTileHeight;
	
	unsigned_int  GfxWidth;
	unsigned_int  GfxHeight;
} GRF_HEADER;

void IconFromGRF() {

	FILE     * GrfFile;
	unsigned char *GrfData;
	unsigned char *GrfPtr;
	unsigned   GrfSize;
	
	GRF_HEADER * GrfHeader;
	unsigned char   *GfxData;
	unsigned char   *PalData;
	
	Banner banner;
	
	// Open File and Read to Memory
	GrfFile = fopen(bannerfilename, "rb");
	if (!GrfFile)
	{
		perror("Cannot open Banner File");
		exit(1);
	}
	
	fseek(GrfFile, 0, SEEK_END);
	GrfSize = ftell(GrfFile);
	fseek(GrfFile, 0, SEEK_SET);
	
	GrfData = (unsigned char *)malloc(GrfSize);
	if (!GrfData)
	{
		fclose(GrfFile);
		fprintf(stderr, "Cannot read Banner File: Out of Memory\n");
		exit(1);
	}
	
	fread(GrfData, 1, GrfSize, GrfFile);
	if (ferror(GrfFile))
	{
		perror("Cannot read Banner File");
		fclose(GrfFile);
		exit(1);
	}
	
	fclose(GrfFile);
	
	// Parse RIFF File Structure : Check File Format
	GrfHeader = NULL;
	GfxData   = NULL;
	PalData   = NULL;
	
	unsigned int datasize = GrfData[4] | (GrfData[5] << 8) | (GrfData[6] << 16) | (GrfData[7] << 24); 

	if ( (memcmp(&GrfData[0], "RIFF", 4) != 0) ||
		 (datasize != GrfSize-8) ||
		 (memcmp(&GrfData[8], "GRF ", 4) != 0) )
	{
		fprintf(stderr, "Banner File Error: File is no GRF File!\n");
		goto error;
	}
	
	GrfPtr = &GrfData[12];
	
	// Parse RIFF File Structure : Read Chunks
	while ((unsigned)(GrfPtr - GrfData) < GrfSize)
	{
		if (memcmp(&GrfPtr[0], "HDR ", 4) == 0)
		{
			GrfHeader =  (GRF_HEADER *)&GrfPtr[8];
		}
		else if (memcmp(&GrfPtr[0], "GFX ", 4) == 0)
		{
			GfxData = &GrfPtr[8];
		}
		else if (memcmp(&GrfPtr[0], "PAL ", 4) == 0)
		{
			PalData = &GrfPtr[8];
		}
		datasize = GrfPtr[4] | (GrfPtr[5] << 8) | (GrfPtr[6] << 16) | (GrfPtr[7] << 24);
		GrfPtr += datasize+8;
	}
	
	// Check Chunks
	if (!GrfHeader || !GfxData || !PalData)
	{
		fprintf(stderr, "Banner File Error: GRF File is incomplete!\n");
		goto error;
	} 
	// Check Header
	// Note: Error checking is probably incomplete
	if (GrfHeader->GfxWidth != 32 && GrfHeader->GfxHeight != 32)
	{
		fprintf(stderr, "Banner File Error: Image must be 32x32pixels!\n");
		goto error;
	}
	if (GrfHeader->GfxAttr != 4)
	{
		fprintf(stderr, "Banner File Error: Image must have 16 colors!\n");
		goto error;
	}
	if (GrfHeader->TileWidth != 8 && GrfHeader->TileHeight != 8)
	{
		fprintf(stderr, 
			"Banner File Error: Image must consist of 8x8 pixel tiles!\n");
		goto error;
	}
	
	// Check Compression
	if (
		((GfxData[0] & 0xF0) != 0x00) ||
		((PalData[0] & 0xF0) != 0x00)
		)
	{
		fprintf(stderr, 
			"Banner File Error: Image must be uncompressed!\n");
		goto error;
	}
	
	// Finally build Banner (Same as IconFromBMP)
	memset(&banner, 0, sizeof(banner));
	banner.version = 1;
	
	// put title
	BannerPutTitle(bannertext, banner);
	
	// put Gfx Data
	memcpy(banner.tile_data, &GfxData[1], 32*16);
	
	// put Pal Data
	memcpy(banner.palette, &PalData[1], 16*2);
	
	// calculate CRC
	for (int slot=0; slot<4; slot++)
	{
		banner.crc[slot] = CalcBannerCRC(banner, slot);
	}
	
	// write to file
	fwrite(&banner, 1, CalcBannerSize(banner.version), fNDS);
	
	// free Memory
	free(GrfData);
	return;
	
	
error:
	free(GrfData);
	exit(1);
}
