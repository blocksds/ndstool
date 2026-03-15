// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#include <errno.h>

#include "log.h"
#include "ndsextract.h"
#include "ndstool.h"
#include "overlay.h"

/*
 * MkDir
 */
void MkDir(const char *name)
{
#ifdef __MINGW32__
	if (mkdir(name))
#else
	if (mkdir(name, S_IRWXU))
#endif
	{
		if (errno != EEXIST)
			LogFatal("Cannot create directory '%s'.\n", name);
	}
}

/*
 * ExtractFile
 * if rootdir==0 nothing will be written
 */
void ExtractFile(const char *rootdir, const char *prefix, const char *entry_name, unsigned int file_id)
{
	long save_filepos = ftell(fNDS);
	if (save_filepos == -1)
		LogFatal("%s: Failed to get position\n", __func__);

	// read FAT data
	if (fseek(fNDS, header.fat_offset + 8*file_id, SEEK_SET) == -1)
		LogFatal("%s: Failed to seek file\n", __func__);

	unsigned_int top;
	if (fread(&top, 1, sizeof(top), fNDS) != sizeof(top))
		LogFatal("%s: Failed to read filesystem data\n", __func__);

	unsigned_int bottom;
	if (fread(&bottom, 1, sizeof(bottom), fNDS) != sizeof(bottom))
		LogFatal("%s: Failed to read bottom\n", __func__);

	unsigned int size = bottom - top;
	if (size > (1U << (17 + header.devicecap)))
	{
		LogFatal("File %u: Size is too big. FAT offset 0x%X contains invalid data.\n",
				file_id, header.fat_offset + 8*file_id);
	}

	// print file info
	if (!rootdir || verbose)
	{
		printf("%5u 0x%08X 0x%08X %9u %s%s\n", file_id, (int)top, (int)bottom, size, prefix, entry_name);
	}

	// extract file
	if (rootdir)
	{
		// make filename
		char filename[MAXPATHLEN];
		strcpy(filename, rootdir);
		strcat(filename, prefix);
		strcat(filename, entry_name);

		if (fseek(fNDS, top, SEEK_SET) == -1)
			LogFatal("%s: Failed to seek top of file\n", __func__);

		FILE *fo = fopen(filename, "wb");
		if (!fo)
			LogFatal("%s: Cannot create file '%s'\n", __func__, filename);

		while (size > 0)
		{
			unsigned char copybuf[1024];
			unsigned int size2 = (size >= sizeof(copybuf)) ? sizeof(copybuf) : size;
			if (fread(copybuf, 1, size2, fNDS) != size2)
				LogFatal("%s: Failed to read data\n", __func__);
			if (fwrite(copybuf, 1, size2, fo) != size2)
				LogFatal("%s: Failed to write data\n", __func__);
			size -= size2;
		}

		fclose(fo);
	}

	if (fseek(fNDS, save_filepos, SEEK_SET) == -1)
		LogFatal("%s: Failed to seek file position\n", __func__);
}

/*
 * MatchName
 */
bool MatchName(char *name, char *mask, int level=0)
{
	char *a = name;
	char *b = mask;
	//for (int i=0; i<level; i++) printf("  "); printf("matching a='%s' b='%s'\n", a, b);
	while (1)
	{
		//for (int i=0; i<level; i++) printf("a=%s b=%s\n", a, b);
		if (*b == '*')
		{
			while (*b == '*') b++;
	//for (int i=0; i<level; i++) printf("  "); printf("* '%s' '%s'\n", a, b);
			bool match = false;
			char *a2 = a;
			do
			{
				if (MatchName(a2, b, level+1)) { a = a2; match = true; break; }
			} while (*a2++);
			if (!match) return false;
			//for (int i=0; i<level; i++) printf("  "); printf("matched a='%s' b='%s'\n", a, b);
		}
		//for (int i=0; i<level; i++) printf("  "); printf("a=%s b=%s\n", a, b);
		if (!*a && !*b) return true;
		if (*a && !*b) return false;
		if (!*a && *b) return false;
		if (*b != '?' && *a != *b) return false;
		a++; b++;
	}
}

/*
 * ExtractDirectory
 * filerootdir can be 0 for just listing files
 */
void ExtractDirectory(const char *filerootdir, const char *prefix, unsigned int dir_id)
{
	char strbuf[MAXPATHLEN];
	long save_filepos = ftell(fNDS);
	if (save_filepos == -1)
		LogFatal("%s: Failed to get position\n", __func__);

	if (fseek(fNDS, header.fnt_offset + 8*(dir_id & 0xFFF), SEEK_SET) == -1)
		LogFatal("%s: Failed to seek directory info\n", __func__);

	unsigned_int entry_start;	// reference location of entry name
	if (fread(&entry_start, 1, sizeof(entry_start), fNDS) != sizeof(entry_start))
		LogFatal("%s: Failed to read entry start ID\n", __func__);

	unsigned_short top_file_id;	// file ID of top entry
	if (fread(&top_file_id, 1, sizeof(top_file_id), fNDS) != sizeof(top_file_id))
		LogFatal("%s: Failed to read top file ID\n", __func__);

	unsigned_short parent_id;	// ID of parent directory or directory count (root)
	if (fread(&parent_id, 1, sizeof(parent_id), fNDS) != sizeof(parent_id))
		LogFatal("%s: Failed to read parent ID\n", __func__);

	if (fseek(fNDS, header.fnt_offset + entry_start, SEEK_SET) == -1)
		LogFatal("%s: Failed to seek directory start\n", __func__);

	// print directory name
	//printf("%04X ", dir_id);
	if (!filerootdir || verbose)
	{
		printf("%s\n", prefix);
	}

	for (unsigned int file_id=top_file_id; ; file_id++)
	{
		unsigned char entry_type_name_length;
		if (fread(&entry_type_name_length, 1, sizeof(entry_type_name_length), fNDS) != sizeof(entry_type_name_length))
			LogFatal("%s: Failed to read entry type name length\n", __func__);

		unsigned int name_length = entry_type_name_length & 127;
		bool entry_type_directory = (entry_type_name_length & 128) ? true : false;
		if (name_length == 0) break;
	
		char entry_name[128];
		memset(entry_name, 0, 128);
		size_t entry_type_name_size = entry_type_name_length & 127;
		if (fread(entry_name, 1, entry_type_name_size, fNDS) != entry_type_name_size)
			LogFatal("%s: Failed to read entry type name\n", __func__);

		if (entry_type_directory)
		{
			unsigned_short dir_id;
			if (fread(&dir_id, 1, sizeof(dir_id), fNDS) != sizeof(dir_id))
				LogFatal("%s: Failed to read directory ID\n", __func__);

			if (filerootdir)
			{
				strcpy(strbuf, filerootdir);
				strcat(strbuf, prefix);
				strcat(strbuf, entry_name);
				MkDir(strbuf);
			}

			strcpy(strbuf, prefix);
			strcat(strbuf, entry_name);
			strcat(strbuf, "/");
			ExtractDirectory(filerootdir, strbuf, dir_id);
		}
		else
		{
			if (1)
			{
				bool match = (filemask_num == 0);
				for (int i=0; i<filemask_num; i++)
				{
					//printf("%s %s\n", entry_name, filemasks[i]);
					if (MatchName(entry_name, filemasks[i])) { match = true; break; }
				}

				//if (!directorycreated)
				//{
				//}
				
				//printf("%d\n", match);

				if (match) ExtractFile(filerootdir, prefix, entry_name, file_id);
			}
		}
	}

	if (fseek(fNDS, save_filepos, SEEK_SET) == -1)
		LogFatal("%s: Failed to seek previous file position\n", __func__);
}

/*
 * ExtractFiles
 */
void ExtractFiles(const char *ndsfilename, const char *filerootdir)
{
	fNDS = fopen(ndsfilename, "rb");
	if (!fNDS)
		LogFatal("Cannot open file '%s'.\n", ndsfilename);

	if (fread(&header, 512, 1, fNDS) != 1)
		LogFatal("%s: Failed to read header\n", __func__);

	if (filerootdir)
		MkDir(filerootdir);

	ExtractDirectory(filerootdir, "/", 0xF000); // list or extract

	fclose(fNDS);
}

/*
 * ExtractOverlayFiles2
 */
 void ExtractOverlayFiles2(unsigned int overlay_offset, unsigned int overlay_size)
 {
 	OverlayEntry overlayEntry;

	if (overlay_size)
	{
		if (fseek(fNDS, overlay_offset, SEEK_SET) == -1)
			LogFatal("%s: Failed to seek overlay offset\n", __func__);

		for (unsigned int i=0; i<overlay_size; i+=sizeof(OverlayEntry))
		{
			if (fread(&overlayEntry, 1, sizeof(overlayEntry), fNDS) != sizeof(overlayEntry))
				LogFatal("%s: Failed to read overlay entry\n", __func__);

			int file_id = overlayEntry.id;
			char s[32]; sprintf(s, OVERLAY_FMT, file_id);
			ExtractFile(overlaydir, "/", s, file_id);
		}
	}
}


/*
 * ExtractOverlayFiles
 */
void ExtractOverlayFiles()
{
	fNDS = fopen(ndsfilename, "rb");
	if (!fNDS)
		LogFatal("Cannot open file '%s'.\n", ndsfilename);

	if (fread(&header, 512, 1, fNDS) != 1)
		LogFatal("%s: Failed to read header\n", __func__);

	if (overlaydir)
	{
		MkDir(overlaydir);
	}

	ExtractOverlayFiles2(header.arm9_overlay_offset, header.arm9_overlay_size);
	ExtractOverlayFiles2(header.arm7_overlay_offset, header.arm7_overlay_size);

	fclose(fNDS);
}

/*
 * Extract
 */
void Extract(const char *outfilename, bool indirect_offset, unsigned int offset, bool indirect_size, unsigned size, bool with_footer)
{
	fNDS = fopen(ndsfilename, "rb");
	if (!fNDS)
		LogFatal("Cannot open file '%s'\n", ndsfilename);

	if (fread(&header, 512, 1, fNDS) != 1)
		LogFatal("%s: Failed to read header\n", __func__);

	if (indirect_offset) offset = *((unsigned_int *)&header + offset/4);
	if (indirect_size) size = *((unsigned_int *)&header + size/4);

	if (fseek(fNDS, offset, SEEK_SET) == -1)
		LogFatal("%s: Failed to seek file offset\n", __func__);

	FILE *fo = fopen(outfilename, "wb");
	if (!fo)
		LogFatal("Cannot create file '%s'.\n", outfilename);

	unsigned char copybuf[1024];
	while (size > 0)
	{
		unsigned int size2 = (size >= sizeof(copybuf)) ? sizeof(copybuf) : size;
		if (fread(copybuf, 1, size2, fNDS) != size2)
			LogFatal("%s: Failed to read buffer\n", __func__);
		if (fwrite(copybuf, 1, size2, fo) != size2)
			LogFatal("%s: Failed to read buffer\n", __func__);
		size -= size2;
	}

	if (with_footer)
	{
		unsigned_int nitrocode;
		if (fread(&nitrocode, sizeof(nitrocode), 1, fNDS) != 1)
			LogFatal("%s: Failed to read nitrocode\n", __func__);

		if (nitrocode == 0xDEC00621)
		{
			// 0x2106C0DE, version info, reserved?
			for (int i=0; i<3; i++)		// write additional 3 words
			{
				if (fwrite(&nitrocode, sizeof(nitrocode), 1, fo) != 1)
					LogFatal("%s: Failed to write data\n", __func__);
				if (fread(&nitrocode, sizeof(nitrocode), 1, fNDS) != 1) // next field
					LogFatal("%s: Failed to read data\n", __func__);
			}
		}
	}

	fclose(fo);
	fclose(fNDS);
}
