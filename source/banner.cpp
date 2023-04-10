#include "ndstool.h"
#include "raster.h"
#include "banner.h"
#include "crc.h"
#include "utf16.h"

unsigned int GetBannerStartCRCSlot(unsigned short slot);
void InsertBannerCRC(Banner &banner, unsigned int bannersize);

const char *bannerLanguages[] = { "Japanese", "English", "French", "German", "Italian", "Spanish", "Chinese", "Korean" };

unsigned short GetBannerMinVersionForCRCSlot(unsigned short slot)
{
	switch(slot)
	{
		case 0: return 0x0001;
		case 1: return 0x0002;
		case 2: return 0x0003;
		case 3: return 0x0103;
		default: return BAD_MIN_VERSION_CRC;
	}
}

unsigned int GetBannerStartCRCSlot(unsigned short slot)
{
	switch(slot)
	{
		case 0:
		case 1:
		case 2: return 0x20;
		case 3: return 0x1240;
		default: return 0;
	}
}

unsigned short CalcBannerCRC(Banner &banner, unsigned short slot, unsigned int bannersize)
{
	unsigned short banner_min_version = GetBannerMinVersionForCRCSlot(slot);

	if (banner_min_version == BAD_MIN_VERSION_CRC || banner.version < banner_min_version)
		return 0;

	unsigned int bannersize_used = CalcBannerSize(banner_min_version);
	unsigned int bannercrc_start = GetBannerStartCRCSlot(slot);

	if (bannersize < bannersize_used)
		bannersize_used = bannersize;

	if (bannercrc_start == 0 || bannersize_used < bannercrc_start)
		return 0;

	return CalcCrc16((unsigned char *)&banner + bannercrc_start, bannersize_used - bannercrc_start);
}

void InsertBannerCRC(Banner &banner, unsigned int bannersize)
{
	for (int slot = 0; slot < NUM_VERSION_CRCS; slot++)
		banner.crc[slot] = CalcBannerCRC(banner, slot, bannersize);
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

void BannerPutTitles(Banner &banner)
{
	for (int l=0; l<GetBannerLanguageCount(banner.version); l++)
	{
		int text_idx = bannertext[l] ? l : 1;
		// convert initial title
		if (!utf16_convert_from_system(bannertext[text_idx], 0, banner.title[l], BANNER_TITLE_LENGTH * 2))
		{
			// avoid repeating error message more than once
			if (l == text_idx)
			{
				fprintf(stderr, "WARNING: UTF-16 conversion failed, using fallback.\n");
			}
			for (int i=0; bannertext[text_idx][i] && (i<BANNER_TITLE_LENGTH); i++)
			{
				banner.title[l][i] = bannertext[text_idx][i];
			}
		}
		banner.title[l][BANNER_TITLE_LENGTH-1] = 0;

		// convert ; to newline
		for (int i=0; banner.title[l][i]; i++)
		{
			if (banner.title[l][i] == ';')
				banner.title[l][i] = 0x0A;
		}
	}
}

unsigned short ExtractBannerVersion(FILE *fNDS, unsigned int banner_offset)
{
	fseek(fNDS, banner_offset, SEEK_SET);
	unsigned short version;
	fread(&version, sizeof(version), 1, fNDS);
	return version;
}

/*
 * FixBannerCRC
 */
void FixBannerCRC(char *ndsfilename, unsigned int banner_offset, unsigned int bannersize)
{
	fNDS = fopen(ndsfilename, "r+b");
	if (!fNDS) { fprintf(stderr, "Cannot open file '%s'.\n", ndsfilename); exit(1); }

	// banner info
	if (banner_offset)
	{
		Banner banner;
		memset(&banner, 0, sizeof(banner));
		fseek(fNDS, banner_offset, SEEK_SET);
		if (fread(&banner, 1, bannersize, fNDS)) {
			InsertBannerCRC(banner, bannersize);
			fseek(fNDS, banner_offset, SEEK_SET);
			fwrite(&banner, bannersize, 1, fNDS);
		}
	}
	fclose(fNDS);
}

static bool IconPrepareValidateBMP(RasterImage &bmp, bool force_zero_transparent)
{
	if (bmp.width != 32 || bmp.height != 32)
	{
		fprintf(stderr, "Icon image should be 32 x 32.\n");
		return false;
	}

	if (bmp.frames <= 0 || bmp.frames > 64)
	{
		fprintf(stderr, "Icon image should have between 1 and 64 frames.\n");
		return false;
	}

	if (!bmp.quantize_rgb15())
	{
		return false;
	}

	if (!bmp.convert_palette())
	{
		return false;
	}

	// if (force_zero_transparent)
	//
	// TODO: The old .BMP format force-set color 0 to transparent.
	// We could keep compatibility here, but that would require
	// parsing the original .BMP file's palette order.
	(void) force_zero_transparent;

	if (!bmp.make_zero_transparent())
	{
		return false;
	}

	if (bmp.max_palette_count() > 16)
	{
		fprintf(stderr, "Icon image should have at most 16 colors.\n");
		return false;
	}

	return true;
}

static void IconRasterToBanner(const RasterImage &bmp, int frame, unsigned char tile_data[4][4][8][4], unsigned_short palette[16])
{
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
					unsigned int b0 = bmp.get_data(frame, col*8 + x, row*8 + y);
					unsigned int b1 = bmp.get_data(frame, col*8 + x + 1, row*8 + y);
					tile_data[row][col][y][x >> 1] = (b1 << 4) | b0;
				}
			}
		}
	}

	// palette
	for (unsigned int i = 0; i < 16; i++)
	{
		if (i < bmp.palette_count[frame])
		{
			palette[i] = RasterRgbQuad(bmp.palette[frame][i]).rgb15();
		}
		else
		{
			palette[i] = 0;
		}
	}
}

static unsigned_short IconGetSequenceEntry(int frame, int frame_entry, int delay, bool flip_h, bool flip_v)
{
	if (delay < 1)
	{
		delay = 1;
	}
	else if (delay > 255)
	{
		fprintf(stderr, "Warning: Frame %d delay %d too long - shortened to 255.\n", frame, delay);
		delay = 255;
	}
	return delay | (frame_entry << 8) | (frame_entry << 11) | (flip_h ? (1 << 14) : 0) | (flip_v ? (1 << 15) : 0);
}

/*
 * IconFromBMP
 */
void IconFromBMP()
{
	RasterImage bmp, bmp_anim;
	if (bannerfilename == NULL && banneranimfilename == NULL)
	{
		// TODO: default image
		fprintf(stderr, "Error: No banner icon image provided!\n");
		exit(1);
	}

	if      (bannerfilename == NULL)     bannerfilename = banneranimfilename;
	else if (banneranimfilename == NULL) banneranimfilename = bannerfilename;

	if (!bmp.load(bannerfilename)) exit(1);
	if (!IconPrepareValidateBMP(bmp, IsBmpExtensionFilename(bannerfilename))) exit(1);

	if (!bmp_anim.load(banneranimfilename)) exit(1);
	if (!IconPrepareValidateBMP(bmp_anim, IsBmpExtensionFilename(banneranimfilename))) exit(1);

	Banner banner;
	memset(&banner, 0, sizeof(banner));
	banner.version = 0x0001;
	if (bannertext[6]) banner.version = 0x0002;
	if (bannertext[7]) banner.version = 0x0003;
	if (bmp_anim.frames > 1 || bmp != bmp_anim) banner.version = 0x0103;
	bannersize = CalcBannerSize(banner.version);

	IconRasterToBanner(bmp, 0, banner.tile_data, banner.palette);

	if (banner.version >= 0x0103)
	{
		// generate animation tiles and sequence

		// TODO: This doesn't support packing tiles which can be expressed as two palettes of the same tile data,
		// or two tile data instances under the same palette data. It supports up to eight unique frames only.

		if (bmp_anim.frames <= 8)
		{
			// fast path
			for (int i = 0; i < bmp_anim.frames; i++)
			{
				IconRasterToBanner(bmp, i, banner.anim_tile_data[i], banner.anim_palette[i]);
				banner.anim_sequence[i] = IconGetSequenceEntry(i, i, bmp.delays[i], false, false);
			}
		}
		else
		{
			// slow path
			int frame_alloc_idx = 0;
			RasterImage frame_alloc[8];
			frame_alloc[frame_alloc_idx++] = bmp_anim.subimage(0);
			IconRasterToBanner(bmp, 0, banner.anim_tile_data[0], banner.anim_palette[0]);
			banner.anim_sequence[0] = IconGetSequenceEntry(0, 0, bmp.delays[0], false, false);

			for (int i = 1; i < bmp_anim.frames; i++)
			{
				int fa_id = -1;
				int fa_variant = 0;
				for (int j = 0; j < frame_alloc_idx; j++)
				{
					for (int v = 0; v < 4; v++)
					{
						if (frame_alloc[j] == bmp_anim.subimage(i).clone((v & 1) != 0, (v & 2) != 0))
						{
							fa_id = j;
							fa_variant = v;
							break;
						}
					}
					if (fa_id >= 0) break;
				}
				if (fa_id < 0)
				{
					if (frame_alloc_idx >= 8)
					{
						fprintf(stderr, "Could not convert animated icon - too many unique frames.\n");
						exit(1);
					}
					fa_id = frame_alloc_idx++;
					fa_variant = 0;
					frame_alloc[fa_id] = bmp_anim.subimage(i);
					IconRasterToBanner(bmp, i, banner.anim_tile_data[fa_id], banner.anim_palette[fa_id]);
				}

				banner.anim_sequence[i] = IconGetSequenceEntry(i, fa_id, bmp.delays[i], (fa_variant & 1) != 0, (fa_variant & 2) != 0);
			}
		}
	}

	BannerPutTitles(banner);
	InsertBannerCRC(banner, bannersize);

	fwrite(&banner, 1, bannersize, fNDS);
}

/*
 * IconFromGRF
 *
 * Assumes input file to be a 32x32 pixel 4bpp tiled image.
 * grit command line:
 *      grit icon.png -g -gt -gB4 -gT <color> -m! -p -pe 16 -fh! -ftr
 *
 * Does not support animated icons.
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
	banner.version = 0x0001;
	if (bannertext[6]) banner.version = 0x0002;
	if (bannertext[7]) banner.version = 0x0003;
	bannersize = CalcBannerSize(banner.version);
	
	// put title
	BannerPutTitles(banner);
	
	// put Gfx Data
	memcpy(banner.tile_data, &GfxData[1], 32*16);
	
	// put Pal Data
	memcpy(banner.palette, &PalData[1], 16*2);
	
	// calculate CRC
	InsertBannerCRC(banner, bannersize);
	
	// write to file
	fwrite(&banner, 1, bannersize, fNDS);
	
	// free Memory
	free(GrfData);
	return;
	
	
error:
	free(GrfData);
	exit(1);
}
