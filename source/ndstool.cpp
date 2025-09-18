// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.
/*
	Nintendo DS rom tool
	by Rafael Vuijk (aka DarkFader)
*/

#include <errno.h>
#include <unistd.h>

#include "ndstool.h"
#include "sha1.h"
#include "ndscreate.h"
#include "ndsextract.h"
#include "banner.h"

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

int filerootdirs_num = 0;
char *filerootdirs[MAX_FILEROOTDIRS];

char *overlaydir = 0;
char *arm7ovltablefilename = 0;
char *arm9ovltablefilename = 0;
const char *bannerfilename = 0;
const char *banneranimfilename = 0;
const char *bannertext[MAX_BANNER_TITLE_COUNT] = {0};
unsigned int bannersize = 0x840;
char *headerfilename_or_size = 0;
char *logofilename = 0;
char *title = 0;
char *makercode = 0;
char *gamecode = 0;
char *vhdfilename = 0;
char *sramfilename = 0;
int latency_1 = 0;
int latency_2 = 24;
int latency1_1 = 2296;
int latency1_2 = 24;
unsigned int romversion = 0;

int bannertype = 0;
unsigned int arm9RamAddress = 0;
unsigned int arm7RamAddress = 0;
unsigned int arm9Entry = 0;
unsigned int arm7Entry = 0;
int unitCode = -1;

// By default declare DSi-compatible games as DSiware. There is no way to create
// a DSi cartridge that works on regular unmodified consoles because they would
// need to be signed. However, some people install DSi ROMs in DSi NAND, where
// the DSiware ID makes more sense. Homebrew loaders generally ignore this
// value, but some of them require the DSiware ID to provide the device list to
// the application when it boots.
//unsigned int titleidHigh = 0x00030000; // DSi-enhanced gamecard
unsigned int titleidHigh = 0x00030004; // DSiware

unsigned int scfgExtMask = 0x80040407; // enable access to everything
unsigned int accessControl = 0x00000138;
unsigned int mbkArm7WramMapAddress = 0;
unsigned int appFlags = 0x01;

void Title()
{
	printf("Nintendo DS rom tool (" VERSION_STRING ")\nby Rafael Vuijk, Dave Murphy, Alexei Karpenko\n");
}

void PrintVersion(void)
{
    printf("ndstool " VERSION_STRING "\n");
}

// Argument information
struct ArgInfo
{
	const char *option_name;
	int required_args;
	const char *help_text;

	void Print()
	{
		char s[1024];
		strcpy(s, help_text);
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

ArgInfo arguments[] =
{
	{"",	0, "Parameter\nSyntax\nComments"},
	{"",	0, "---------\n------\n--------"},
	{"?",   0, "Show this help:\n-?\nPrint help message."},
	{"V",   0, "Show version\n-V\nPrints the version string and exits"},
	{"i",   0, "Show information:\n-i [file.nds]\nHeader information."},
	{"fh",  0, "Fix header CRC\n-fh [file.nds]\nYou only need this after manual editing."},
	{"fb",  0, "Fix banner CRC\n-fb [file.nds]\nYou only need this after manual editing."},
	{"l",   0, "List files:\n-l [file.nds]\nGive a list of contained files."},
	{"c",   0, "Create\n-c [file.nds]"},
	{"x",   0, "Extract\n-x [file.nds]"},
	{"v",   0, "  Show more info\n-v\nShow filenames and more header info"},
	{"vv",  0, "  Show more info\n-vv\nShow even more information than -v"},
	{"9",   1, "  ARM9 executable\n-9 file.bin"},
	{"9i",  1, "  ARM9i executable\n-9i file.bin"},
	{"7",   1, "  ARM7 executable\n-7 file.bin"},
	{"7i",  1, "  ARM7i executable\n-7i file.bin"},
	{"y9",  1, "  ARM9 overlay table\n-y9 file.bin"},
	{"y7",  1, "  ARM7 overlay table\n-y7 file.bin"},
	{"d",   1, "  NitroFS root folder\n-d directory1 <directory2> ...\nAll directories are combined in the root of the filesystem"},
	{"y",   1, "  Overlay files\n-y directory"},
	{"b",   1, "  Banner icon/text\n-b file.[bmp|gif|png] \"text;text;text\"\nThe three lines are shown at different sizes."},
	{"ba",  1, "  Banner animated icon\n-ba file.[bmp|gif|png]"},
	{"bi",  1, "  Banner static icon\n-bi file.[bmp|gif|png]"},
	{"bt",  2, "  Banner text\n-bt [langid] \"region;specific;text\""},
	{"t",   1, "  Banner binary\n-t file.bin"},
	{"h",   1, "  Header size/template\n-h size/file.bin\nIf a ROM file is provided, it uses that file as template for the header. If a size is provided, it sets the size of the header. A header size of 0x4000 is default for real cards and newer homebrew, 0x200 for older homebrew."},
	{"n",   2, "  Latency\n-n [start gap] [per-block gap]\ndefault = 0 24"},
	{"n1",  2, "  Latency 1\n-n1 [start gap] [per-block gap]\ndefault = 2296 24"},
	{"o",   1, "  Logo image/binary\n-o file.[bmp|gif|png]/file.bin"},
	{"m",   1, "  Maker code\n-m code\n The code must be 2 characters long"},
	{"g",   1, "  Game info\n-g gamecode [makercode] [title] [rom ver]\nSets game-specific information.\nGame code is 4 characters. Maker code is 2 characters.\nTitle can be up to 12 characters."},
	{"r9",  1, "  ARM9 RAM address\n-r9 address"},
	{"r7",  1, "  ARM7 RAM address\n-r7 address"},
	{"e9",  1, "  ARM9 RAM entry\n-e9 address"},
	{"e7",  1, "  ARM7 RAM entry\n-e7 address"},
	{"w",   0, "  Wildcard filemask(s)\n-w [filemask]...\n* and ? are wildcard characters."},
	{"u",   1, "  DSi high title ID\n-u tidhigh  (32-bit hex)"},
	{"uc",  1, "  DSi unit code\n-uc unitcode (0 = DS-only, 2 = DS or DSi, 3 = DSi-only)"},
	{"z",   1, "  ARM7 SCFG EXT mask\n-z scfgmask (32-bit hex)"},
	{"a",   1, "  DSi access flags\n-a accessflags (32-bit hex)"},
	{"p",   1, "  DSi application flags\n-p appflags (8-bit hex)"},
	{"q",   1, "  DSi ARM7 WRAM_A map address\n-m address (32-bit hex)"},
	{NULL,  0, NULL} // Marker of end of list
};

ArgInfo *GetArgumentInformation(const char *str)
{
	ArgInfo *arg = &arguments[0];

	while (arg->option_name != NULL)
	{
		if (strcmp(str, arg->option_name) == 0)
			return arg;
		arg++;
	}

	return NULL;
}

void Help(void)
{
	Title();
	printf("\n");

	ArgInfo *arg = &arguments[0];
	while (arg->option_name != NULL)
	{
		arg->Print();
		arg++;
	}

	printf("\n");
	printf("You only need to specify the NDS filename once if you want to perform multiple actions.\n");
	printf("Actions are performed in the specified order.\n");
	printf("Addresses can be prefixed with '0x' to use hexadecimal format.\n");
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

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		Help();
		return 0;
	}

	int num_actions = 0;
	int actions[MAX_ACTIONS];

	int a = 1;
	while (a < argc)
	{
		char *arg = argv[a++];

		if (arg[0] == '-')
		{
			ArgInfo *info = GetArgumentInformation(arg + 1); // Skip dash
			if (info == NULL)
			{
				fprintf(stderr, "Unknown argument '%s'\n", arg);
				return EXIT_FAILURE;
			}

			if (info->required_args > 0)
			{
				if (a + info->required_args > argc)
				{
					fprintf(stderr, "Not enough arguments for '%s'\n", arg);
					return EXIT_FAILURE;
				}
			}
		}
		else
		{
			// This is a positional argument. There is only one positional
			// argument supported.
			if (ndsfilename == NULL)
			{
				ndsfilename = arg;
				continue;
			}

			fprintf(stderr, "Unknown argument (use -? for help): %s\n", arg);
			return EXIT_FAILURE;
		}

		if (strcmp(arg, "-i") == 0) // Show information
		{
			ADDACTION(ACTION_SHOWINFO);
			if (argc > a && argv[a][0] != '-')
				ndsfilename = argv[a++];
		}
		else if (strcmp(arg, "-fh") == 0) // Fix header CRC
		{
			ADDACTION(ACTION_FIXHEADERCRC);
			if (argc > a && argv[a][0] != '-')
				ndsfilename = argv[a++];
		}
		else if (strcmp(arg, "-fb") == 0) // Fix banner CRC
		{
			ADDACTION(ACTION_FIXBANNERCRC);
			if (argc > a && argv[a][0] != '-')
				ndsfilename = argv[a++];
		}
		else if (strcmp(arg, "-l") == 0) // List files
		{
			ADDACTION(ACTION_LISTFILES);
			if (argc > a && argv[a][0] != '-')
				ndsfilename = argv[a++];
		}
		else if (strcmp(arg, "-x") == 0) // Extract
		{
			ADDACTION(ACTION_EXTRACT);
			if (argc > a && argv[a][0] != '-')
				ndsfilename = argv[a++];
		}
		else if (strcmp(arg, "-w") == 0) // Wildcard filemasks
		{
			while (1)
			{
				// No more arguments
				if (argc == a)
					break;

				// Detect next program option and exit loop
				if (argv[a][0] == '-')
					break;

				if (filemask_num == MAX_FILEMASKS)
				{
					fprintf(stderr, "Too many file masks\n");
					return EXIT_FAILURE;
				}

				filemasks[filemask_num++] = argv[a++];
			}
		}
		else if (strcmp(arg, "-c") == 0) // Create
		{
			ADDACTION(ACTION_CREATE);
			if (argc > a && argv[a][0] != '-')
				ndsfilename = argv[a++];
		}
		else if (strcmp(arg, "-d") == 0) // File root directory
		{
			while (1)
			{
				// No more arguments
				if (argc == a)
					break;

				// Detect next program option and stop
				if (argv[a][0] == '-')
					break;

				if (filerootdirs_num == MAX_FILEROOTDIRS)
				{
					fprintf(stderr, "Too many root directories");
					return EXIT_FAILURE;
				}

				filerootdirs[filerootdirs_num++] = argv[a++];
			}
		}
		else if (strcmp(arg, "-7i") == 0) // ARM7i filename
		{
			arm7ifilename = argv[a++];
		}
		else if (strcmp(arg, "-7") == 0) // ARM7 filename
		{
			arm7filename = argv[a++];
		}
		else if (strcmp(arg, "-9i") == 0) // ARM9i filename
		{
			arm9ifilename = argv[a++];
		}
		else if (strcmp(arg, "-9") == 0) // ARM9 filename
		{
			arm9filename = argv[a++];
		}
		else if (strcmp(arg, "-t") == 0)
		{
			bannertype = BANNER_BINARY;
			bannerfilename = argv[a++];
		}
		else if (strcmp(arg, "-bt") == 0)
		{
			bannertype = BANNER_IMAGE;

			unsigned int text_idx = strtoul(argv[a++], 0, 0);

			if (errno == EINVAL)
			{
				text_idx = 1;
			}
			else if (text_idx >= MAX_BANNER_TITLE_COUNT)
			{
				fprintf(stderr, "The argument for '-bt' must be a number between 0 and %d\n",
						MAX_BANNER_TITLE_COUNT - 1);
				return EXIT_FAILURE;
			}

			bannertext[text_idx] = argv[a++];
		}
		else if (strcmp(arg, "-bi") == 0)
		{
			bannertype = BANNER_IMAGE;
			bannerfilename = argv[a++];
		}
		else if (strcmp(arg, "-ba") == 0)
		{
			bannertype = BANNER_IMAGE;
			banneranimfilename = argv[a++];
		}
		else if (strcmp(arg, "-b") == 0)
		{
			bannertype = BANNER_IMAGE;

			bannerfilename = argv[a++];

			if (argc > a && argv[a][0] != '-')
				bannertext[1] = argv[a++];
		}
		else if (strcmp(arg, "-o") == 0)
		{
			logofilename = argv[a++];
		}
		else if (strcmp(arg, "-h") == 0) // Load header or header size
		{
			headerfilename_or_size = argv[a++];
		}
		else if (strcmp(arg, "-u") == 0) // DSi title ID high word
		{
			titleidHigh = strtoul(argv[a++], 0, 16);
		}
		else if (strcmp(arg, "-uc") == 0) // DS unit code
		{
			// Valid unit codes are 0 (DS-only), 2 (DS and DSi supported) and 3
			// (DSi-only).
			unitCode = strtoul(argv[a++], 0, 10);
			if ((unitCode == 1) || (unitCode > 3))
			{
				fprintf(stderr, "Invalid value for '-uc' (must be 0, 2 or 3): %u\n", unitCode);
				return EXIT_FAILURE;
			}
		}
		else if (strcmp(arg, "-z") == 0) // SCFG access flags
		{
			scfgExtMask = strtoul(argv[a++], 0, 16);
		}
		else if (strcmp(arg, "-a") == 0) // DSi access control flags
		{
			accessControl = strtoul(argv[a++], 0, 16);
		}
		else if (strcmp(arg, "-p") == 0) // DSi application flags
		{
			appFlags = strtoul(argv[a++], 0, 16) & 0xFF;
		}
		else if (strcmp(arg, "-q") == 0) // DSi ARM7 WRAM_A map address
		{
			mbkArm7WramMapAddress = strtoul(argv[a++], 0, 16);
		}
		else if (strcmp(arg, "-V") == 0) // Version string
		{
			PrintVersion();
			return EXIT_SUCCESS;
		}
		else if (strcmp(arg, "-v") == 0) // Verbose
		{
			verbose = 1;
		}
		else if (strcmp(arg, "-vv") == 0) // More verbose
		{
			verbose = 2;
		}
		else if (strcmp(arg, "-n") == 0) // Latency
		{
			latency_1 = strtoul(argv[a++], 0, 0);
			latency_2 = strtoul(argv[a++], 0, 0);
		}
		else if (strcmp(arg, "-n1") == 0) // Latency
		{
			latency1_1 = strtoul(argv[a++], 0, 0);
			latency1_2 = strtoul(argv[a++], 0, 0);
		}
		else if (strcmp(arg, "-r7") == 0) // ARM7 RAM address
		{
			arm7RamAddress = strtoul(argv[a++], 0, 0);
		}
		else if (strcmp(arg, "-r9") == 0) // ARM9 RAM address
		{
			arm9RamAddress = strtoul(argv[a++], 0, 0);
		}
		else if (strcmp(arg, "-e7") == 0) // ARM7 entrypoint
		{
			arm7Entry = strtoul(argv[a++], 0, 0);
		}
		else if (strcmp(arg, "-e9") == 0) // ARM9 entrypoint
		{
			arm9Entry = strtoul(argv[a++], 0, 0);
		}
		else if (strcmp(arg, "-m") == 0) // Maker code
		{
			makercode = argv[a++];
		}
		else if (strcmp(arg, "-g") == 0) // Game code
		{
			gamecode = argv[a++];
			if (argc > a && argv[a][0] != '-')
			{
				makercode = argv[a++];
				if (argc > a && argv[a][0] != '-')
				{
					title = argv[a++];
					if (argc > a && argv[a][0] != '-')
						romversion = strtoul(argv[a++], 0, 0);
				}
			}
		}
		else if (strcmp(arg, "-y7") == 0) // ARM7 overlay table file
		{
			arm7ovltablefilename = argv[a++];
		}
		else if (strcmp(arg, "-y9") == 0) // ARM9 overlay table file
		{
			arm9ovltablefilename = argv[a++];
		}
		else if (strcmp(arg, "-y") == 0) // Overlay table directory
		{
			overlaydir = argv[a++];
		}
		else if (strcmp(arg, "-?") == 0) // Global help
		{
			Help();
			return 0; // Do not perform any other actions
		}
		else
		{
			fprintf(stderr, "Internal parser error (argument %d)\n", a);
			return EXIT_FAILURE;
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
	if (romversion > 255)
	{
		fprintf(stderr, "romversion can only be 0 - 255!\n");
		return 1;
	}
	if (!bannertext[1])
		bannertext[1] = "";

	/*
	 * perform actions
	 */

	if ((num_actions > 0) && (ndsfilename == NULL))
	{
		fprintf(stderr, "No NDS file provided\n");
		exit(EXIT_FAILURE);
	}

	int status = 0;
	for (int i=0; i<num_actions; i++)
	{
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
				if (!fNDS)
				{
					fprintf(stderr, "Cannot open file '%s'.\n", ndsfilename);
					exit(EXIT_FAILURE);
				}
				FullyReadHeader(fNDS, header);
				bannersize = GetBannerSizeFromHeader(header, ExtractBannerVersion(fNDS, header.banner_offset));
				fclose(fNDS);
				FixBannerCRC(ndsfilename, header.banner_offset, bannersize);
				break;

			case ACTION_EXTRACT: {
				fNDS = fopen(ndsfilename, "rb");
				if (!fNDS)
				{
					fprintf(stderr, "Cannot open file '%s'.\n", ndsfilename);
					exit(EXIT_FAILURE);
				}
				unsigned int headersize = FullyReadHeader(fNDS, header);
				bannersize = GetBannerSizeFromHeader(header, ExtractBannerVersion(fNDS, header.banner_offset));
				fclose(fNDS);

				if (arm9filename) Extract(arm9filename, true, 0x20, true, 0x2C, true);
				if (arm7filename) Extract(arm7filename, true, 0x30, true, 0x3C);
				if (header.unitcode & 2) {
					if (arm9ifilename) Extract(arm9ifilename, true, 0x1C0, true, 0x1CC, true);
					if (arm7ifilename) Extract(arm7ifilename, true, 0x1D0, true, 0x1DC);
				}
				if (bannerfilename) {
					if (bannertype == BANNER_BINARY)
						Extract(bannerfilename, true, 0x68, false, bannersize);
					else if (bannertype == BANNER_IMAGE)
						IconToBMP();
				}
				if (headerfilename_or_size) Extract(headerfilename_or_size, false, 0x0, false, headersize);
				if (logofilename) Extract(logofilename, false, 0xC0, false, 156);	// *** bin only
				if (arm9ovltablefilename) Extract(arm9ovltablefilename, true, 0x50, true, 0x54);
				if (arm7ovltablefilename) Extract(arm7ovltablefilename, true, 0x58, true, 0x5C);
				if (overlaydir) ExtractOverlayFiles();
				if (filerootdirs_num > 0) ExtractFiles(ndsfilename, filerootdirs[0]);
				break;
			}

			case ACTION_CREATE:
				Create();
				break;

			case ACTION_LISTFILES:
				ExtractFiles(ndsfilename, NULL); // List mode
				break;
		}
	}

	return (status < 0) ? 1 : 0;
}
