// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.
/*
	Nintendo DS rom tool
	by Rafael Vuijk (aka DarkFader)
*/

#include <unistd.h>
#include "ndstool.h"
#include "sha1.h"
#include "ndscreate.h"
#include "ndsextract.h"
#include "banner.h"

/*
 * Variables
 */
int verbose = 0;
Header header;
FILE *fNDS = 0;
char *filemasks[MAX_FILEMASKS];
int filemask_num = 0;
char *ndsfilename = 0;
char *arm7filename = 0;
char *arm9filename = 0;
char *arm7ifilename = 0;
char *arm9ifilename = 0;
char *filerootdir = 0;
char *overlaydir = 0;
char *arm7ovltablefilename = 0;
char *arm9ovltablefilename = 0;
char *bannerfilename = 0;
char *banneranimfilename = 0;
const char *bannertext[MAX_BANNER_TITLE_COUNT] = {0};
unsigned int bannersize = 0x840;
char *headerfilename_or_size = 0;
char *logofilename = 0;
char *title = 0;
char *makercode = 0;
char *gamecode = 0;
char *vhdfilename = 0;
char *sramfilename = 0;
int latency1 = 0x1FFF;	//0x8F8;
int latency2 = 0x3F;	//0x18;
unsigned int romversion = 0;

int bannertype = 0;
unsigned int arm9RamAddress = 0;
unsigned int arm7RamAddress = 0;
unsigned int arm9Entry = 0;
unsigned int arm7Entry = 0;
unsigned int titleidHigh = 0x00030000; // DSi-enhanced gamecard. 0x00030004 (DSiWare) cannot be loaded as a card from DSi Menu
unsigned int scfgExtMask = 0x80040407; // enable access to everything
unsigned int accessControl = 0x00000138;
unsigned int mbkArm7WramMapAddress = 0;
unsigned int appFlags = 0x01;


/*
 * Title
 */
void Title()
{
	printf("Nintendo DS rom tool " PACKAGE_VERSION " - %s\nby Rafael Vuijk, Dave Murphy, Alexei Karpenko\n",CompileDate);
}

/*
 * Help data
 */
struct HelpLine
{
	const char *option_char;
	const char *text;

	void Print()
	{
		char s[1024];
		strcpy(s, text);
		printf("%-22s ", strtok(s, "\n"));
		char *p = strtok(0, "\n"); if (p) printf("%-30s ", p);
		for (int i=0; ; i++)
		{
			char *p = strtok(0, "\n");
			if (!p) { printf("\n"); break; }
			if (i) printf("\n%54s", "");
			printf("%s", p);
		}
	}
};

HelpLine helplines[] =
{
	{"",	"Parameter\nSyntax\nComments"},
	{"",	"---------\n------\n--------"},
	{"?",	"Show this help:\n-?[option]\nAll or single help for an option."},
	{"i",	"Show information:\n-i [file.nds]\nHeader information."},
	{"f",	"Fix header CRC\n-fh [file.nds]\nYou only need this after manual editing."},
	{"f",	"Fix banner CRC\n-fb [file.nds]\nYou only need this after manual editing."},
	{"l",	"List files:\n-l [file.nds]\nGive a list of contained files."},
	{"v",	"  Show offsets/sizes\n-v"},
	{"c",	"Create\n-c [file.nds]"},
	{"x",	"Extract\n-x [file.nds]"},
	{"v",	"  Show more info\n-v[v...]\nShow filenames and more header info.\nUse multiple v's for more information."},
	{"9",	"  ARM9 executable\n-9 file.bin"},
	{"7",	"  ARM7 executable\n-7 file.bin"},
	{"y9",	"  ARM9 overlay table\n-y9 file.bin"},
	{"y7",	"  ARM7 overlay table\n-y7 file.bin"},
	{"d",	"  Data files\n-d directory"},
	{"y",	"  Overlay files\n-y directory"},
	{"b",	"  Banner icon/text\n-b file.[bmp|gif|png] \"text;text;text\"\nThe three lines are shown at different sizes."},
	{"b",	"  Banner animated icon\n-ba file.[bmp|gif|png]"},
	{"b",	"  Banner static icon\n-bi file.[bmp|gif|png]"},
	{"b",	"  Banner text\n-bt0 \"region;specific;text\""},
	{"t",	"  Banner binary\n-t file.bin"},
	{"h",	"  Header template\n-h file.bin\nUse the header from another ROM as a template."},
	{"h",	"  Header size\n-h size\nA header size of 0x4000 is default for real cards and newer homebrew, 0x200 for older homebrew."},
	{"n",	"  Latency\n-n [L1] [L2]\ndefault=maximum"},
	{"o",	"  Logo image/binary\n-o file.[bmp|gif|png]/file.bin"},
	{"g",	"  Game info\n-g gamecode [makercode] [title] [rom ver]\nSets game-specific information.\nGame code is 4 characters. Maker code is 2 characters.\nTitle can be up to 12 characters."},
	{"r",	"  ARM9 RAM address\n-r9 address"},
	{"r",	"  ARM7 RAM address\n-r7 address"},
	{"e",	"  ARM9 RAM entry\n-e9 address"},
	{"e",	"  ARM7 RAM entry\n-e7 address"},
	{"w",	"  Wildcard filemask(s)\n-w [filemask]...\n* and ? are wildcard characters."},
	{"u",	"  DSi high title ID\n-u tidhigh  (32-bit hex)"},
	{"z",   "  ARM7 SCFG EXT mask\n-z scfgmask (32-bit hex)"},
	{"a",   "  DSi access flags\n-a accessflags (32-bit hex)"},
	{"p",   "  DSi application flags\n-p appflags (8-bit hex)"},
	{"q",   "  DSi ARM7 WRAM_A map address\n-m address (32-bit hex)"},
};

/*
 * Help
 */
void Help(char *specificoption = 0)
{
	Title();
	printf("\n");

	if (specificoption)
	{
		bool found = false;
		for (unsigned int i = 0; i < (sizeof(helplines) / sizeof(helplines[0])); i++)
		{
			for (const char *o = helplines[i].option_char; *o; o++)
			{
				if (strncmp(specificoption, o, strlen(specificoption)) == 0)
				{
					helplines[i].Print();
					found = true;
				}
			}
		}
		if (!found)
		{
			printf("Unknown option: %s\n\n", specificoption);
		}
	}
	else
	{
		for (unsigned int i=0; i<(sizeof(helplines) / sizeof(helplines[0])); i++)
		{
			helplines[i].Print();
		}
		printf("\n");
		printf("You only need to specify the NDS filename once if you want to perform multiple actions.\n");
		printf("Actions are performed in the specified order.\n");
		printf("Addresses can be prefixed with '0x' to use hexadecimal format.\n");
	}
}

// Must precede OPTIONAL
#define REQUIRED(var)	var = ((argc > a + 1) ? argv[++a] : 0)

// Final parameter requirement checks are done when performing actions
#define OPTIONAL(var)	{ \
	char *t = ((argc > a + 1) && (argv[a + 1][0] != '-') ? argv[++a] : 0); \
	if (!var) \
		var = t; \
	else if (t) \
		fprintf(stderr, "%s is already specified!\n", #var); \
}

// Like OPTIONAL, but for (positive) integers
#define OPTIONAL_INT(var)	{ \
	char *t = ((argc > a + 1) && (argv[a + 1][0] != '-') ? argv[++a] : 0); \
	if (t) \
		var = strtoul(t, 0, 0); \
}

#define MAX_ACTIONS		32
#define ADDACTION(a)	{ if (num_actions < MAX_ACTIONS) actions[num_actions++] = a; }

enum {
	ACTION_SHOWINFO,
	ACTION_FIXHEADERCRC,
	ACTION_FIXBANNERCRC,
	ACTION_LISTFILES,
	ACTION_EXTRACT,
	ACTION_CREATE,
};

/*
 * main
 */
int main(int argc, char *argv[])
{
	#ifdef _NDSTOOL_P_H
		if (sizeof(Header) != 0x200) { fprintf(stderr, "Header size %d != %d\n", sizeof(Header), 0x200); return 1; }
	#endif

	if (argc < 2)
	{
		Help();
		return 0;
	}

	int num_actions = 0;
	int actions[MAX_ACTIONS];

	/*
	 * parse parameters to actions
	 */

	for (int a = 1; a < argc; a++)
	{
		if (strcmp(argv[a], "-i") == 0) // Show information
		{
			ADDACTION(ACTION_SHOWINFO);
			OPTIONAL(ndsfilename);
		}
        else if (strcmp(argv[a], "-fh") == 0) // Fix header CRC
		{
			ADDACTION(ACTION_FIXHEADERCRC);
			OPTIONAL(ndsfilename);
		}
        else if (strcmp(argv[a], "-fb") == 0) // Fix banner CRC
		{
			ADDACTION(ACTION_FIXBANNERCRC);
			OPTIONAL(ndsfilename);
		}
		else if (strcmp(argv[a], "-l") == 0) // List files
		{
			ADDACTION(ACTION_LISTFILES);
			OPTIONAL(ndsfilename);
		}
		else if (strcmp(argv[a], "-x") == 0) // Extract
		{
			ADDACTION(ACTION_EXTRACT);
			OPTIONAL(ndsfilename);
		}
		else if (strcmp(argv[a], "-w") == 0) // Wildcard filemasks
		{
			while (1)
			{
				char *filemask = 0;
				OPTIONAL(filemask);
				if (!(filemasks[filemask_num] = filemask))
					break;
				if (++filemask_num >= MAX_FILEMASKS)
					return 1;
			}
		}
		else if (strcmp(argv[a], "-c") == 0) // Create
		{
			ADDACTION(ACTION_CREATE);
			OPTIONAL(ndsfilename);
		}
		else if (strcmp(argv[a], "-d") == 0) // File root directory
		{
			REQUIRED(filerootdir);
		}
		else if (strcmp(argv[a], "-7i") == 0) // ARM7i filename
		{
			REQUIRED(arm7ifilename);
		}
		else if (strcmp(argv[a], "-7") == 0) // ARM7 filename
		{
			REQUIRED(arm7filename);
		}
		else if (strcmp(argv[a], "-9i") == 0) // ARM9i filename
		{
			REQUIRED(arm9ifilename);
		}
		else if (strcmp(argv[a], "-9") == 0) // ARM9 filename
		{
			REQUIRED(arm9filename);
		}
		else if (strcmp(argv[a], "-t") == 0)
		{
			REQUIRED(bannerfilename);
			bannertype = BANNER_BINARY;
		}
		else if (strncmp(argv[a], "-bt", 3) == 0)
		{
			bannertype = BANNER_IMAGE;
			int text_idx = 1;
			if (argv[a][3] >= '0' && argv[a][3] <= '9')
				text_idx = atoi(argv[a] + 3);

			if (text_idx <= MAX_BANNER_TITLE_COUNT)
			{
				REQUIRED(bannertext[text_idx]);
			}
			else
			{
				char *skip;
				REQUIRED(skip);
				(void) skip;
			}
		}
		else if (strcmp(argv[a], "-bi") == 0)
		{
			bannertype = BANNER_IMAGE;
			REQUIRED(bannerfilename);
		}
		else if (strcmp(argv[a], "-ba") == 0)
		{
			bannertype = BANNER_IMAGE;
			REQUIRED(banneranimfilename);
		}
		else if (strcmp(argv[a], "-b") == 0)
		{
			bannertype = BANNER_IMAGE;
			REQUIRED(bannerfilename);
			banneranimfilename = bannerfilename;
			OPTIONAL(bannertext[1]);
		}
		else if (strcmp(argv[a], "-o") == 0)
		{
			REQUIRED(logofilename);
		}
		else if (strcmp(argv[a], "-h") == 0) // Load header or header size
		{
			REQUIRED(headerfilename_or_size);
		}
		else if (strcmp(argv[a], "-u") == 0) // DSi title ID high word
		{
			if (argc > a)
				titleidHigh = strtoul(argv[++a], 0, 16);
		}
		else if (strcmp(argv[a], "-z") == 0) // SCFG access flags
		{
			if (argc > a)
				scfgExtMask = strtoul(argv[++a], 0, 16);
		}
		else if (strcmp(argv[a], "-a") == 0) // DSi access control flags
		{
			if (argc > a)
				accessControl = strtoul(argv[++a], 0, 16);
		}
		else if (strcmp(argv[a], "-p") == 0) // DSi application flags
		{
			if (argc > a)
				appFlags = strtoul(argv[++a], 0, 16) & 0xFF;
		}
		else if (strcmp(argv[a], "-q") == 0) // DSi ARM7 WRAM_A map address
		{
			if (argc > a)
				mbkArm7WramMapAddress = strtoul(argv[++a], 0, 16);
		}
		else if (strcmp(argv[a], "-v") == 0) // Verbose
		{
			verbose = 1;
		}
		else if (strcmp(argv[a], "-vv") == 0) // More verbose
		{
			verbose = 2;
		}
		else if (strcmp(argv[a], "-n") == 0) // Latency
		{
			OPTIONAL_INT(latency1);
			OPTIONAL_INT(latency2);
		}
		else if (strcmp(argv[a], "-r7") == 0) // ARM7 RAM address
		{
			arm7RamAddress = (argc > a) ? strtoul(argv[++a], 0, 0) : 0;
		}
		else if (strcmp(argv[a], "-r9") == 0) // ARM9 RAM address
		{
			arm9RamAddress = (argc > a) ? strtoul(argv[++a], 0, 0) : 0;
		}
		else if (strcmp(argv[a], "-e7") == 0) // ARM7 entrypoint
		{
			arm7Entry = (argc > a) ? strtoul(argv[++a], 0, 0) : 0;
		}
		else if (strcmp(argv[a], "-e9") == 0) // ARM9 entrypoint
		{
			arm9Entry = (argc > a) ? strtoul(argv[++a], 0, 0) : 0;
		}
		else if (strcmp(argv[a], "-m") == 0) // Maker code
		{
			REQUIRED(makercode);
		}
		else if (strcmp(argv[a], "-g") == 0) // Game code
		{
			REQUIRED(gamecode);
			OPTIONAL(makercode);
			OPTIONAL(title);
			OPTIONAL_INT(romversion);
		}
		else if (strcmp(argv[a], "-y7") == 0) // ARM7 overlay table file
		{
			REQUIRED(arm7ovltablefilename);
		}
		else if (strcmp(argv[a], "-y9") == 0) // ARM9 overlay table file
		{
			REQUIRED(arm9ovltablefilename);
		}
		else if (strcmp(argv[a], "-y") == 0) // Overlay table directory
		{
			REQUIRED(overlaydir);
		}
		else if (strcmp(argv[a], "-?") == 0) // Global or specific help
		{
			char *helpoption = 0;
			OPTIONAL(helpoption);
			Help(helpoption);
			return 0; // Do not perform any other actions
		}
		else
		{
			if (ndsfilename)
			{
				fprintf(stderr, "NDS filename is already given!\n");
				return 1;
			}
			ndsfilename = argv[a];
			break;
		}
	}

	Title();

	/*
	 * sanity checks
	 */

	if (gamecode)
	{
		if (strlen(gamecode) != 4)
		{
			fprintf(stderr, "Game code must be 4 characters!\n");
			return 1;
		}
		for (int i=0; i<4; i++) if ((gamecode[i] >= 'a') && (gamecode[i] <= 'z'))
		{
			fprintf(stderr, "Warning: Gamecode contains lowercase characters.\n");
			break;
		}
		if (gamecode[0] == 'A')
		{
			fprintf(stderr, "Warning: Gamecode starts with 'A', which might be used for another commercial product.\n");
		}
	}
	if (makercode && (strlen(makercode) != 2))
	{
		fprintf(stderr, "Maker code must be 2 characters!\n");
		return 1;
	}
	if (title && (strlen(title) > 12))
	{
		fprintf(stderr, "Title can be no more than 12 characters!\n");
		return 1;
	}
	if (romversion > 255) {
		fprintf(stderr, "romversion can only be 0 - 255!\n");
		return 1;
	}
	if (!bannertext[1]) {
		bannertext[1] = "";
	}

	/*
	 * perform actions
	 */

	int status = 0;
	for (int i=0; i<num_actions; i++)
	{
//printf("action %d\n", actions[i]);
		switch (actions[i])
		{
			case ACTION_SHOWINFO:
				ShowInfo(ndsfilename);
				break;

			case ACTION_FIXHEADERCRC:
				FixHeaderCRC(ndsfilename);
				break;

			case ACTION_FIXBANNERCRC:
				fNDS = fopen(ndsfilename, "rb");
				if (!fNDS) { fprintf(stderr, "Cannot open file '%s'.\n", ndsfilename); exit(1); }
				FullyReadHeader(fNDS, header);
				bannersize = GetBannerSizeFromHeader(header, ExtractBannerVersion(fNDS, header.banner_offset));
				fclose(fNDS);
				FixBannerCRC(ndsfilename, header.banner_offset, bannersize);
				break;

			case ACTION_EXTRACT: {
				fNDS = fopen(ndsfilename, "rb");
				if (!fNDS) { fprintf(stderr, "Cannot open file '%s'.\n", ndsfilename); exit(1); }
				unsigned int headersize = FullyReadHeader(fNDS, header);
				bannersize = GetBannerSizeFromHeader(header, ExtractBannerVersion(fNDS, header.banner_offset));
				fclose(fNDS);

				if (arm9filename) Extract(arm9filename, true, 0x20, true, 0x2C, true);
				if (arm7filename) Extract(arm7filename, true, 0x30, true, 0x3C);
				if (header.unitcode & 2) {
					if (arm9ifilename) Extract(arm9ifilename, true, 0x1C0, true, 0x1CC, true);
					if (arm7ifilename) Extract(arm7ifilename, true, 0x1D0, true, 0x1DC);
				}
				if (bannerfilename) Extract(bannerfilename, true, 0x68, false, bannersize);
				if (headerfilename_or_size) Extract(headerfilename_or_size, false, 0x0, false, headersize);
				if (logofilename) Extract(logofilename, false, 0xC0, false, 156);	// *** bin only
				if (arm9ovltablefilename) Extract(arm9ovltablefilename, true, 0x50, true, 0x54);
				if (arm7ovltablefilename) Extract(arm7ovltablefilename, true, 0x58, true, 0x5C);
				if (overlaydir) ExtractOverlayFiles();
				if (filerootdir) ExtractFiles(ndsfilename);
				break;
			}

			case ACTION_CREATE:
				Create();
				break;

			case ACTION_LISTFILES:
				filerootdir = 0;
				/*status =*/ ExtractFiles(ndsfilename);
				break;
		}
	}

	return (status < 0) ? 1 : 0;
}
