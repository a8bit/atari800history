/*
 * ==========================
 * Atari800 - Amiga Port Code
 * ==========================
 */

/*
 * Revision     : v0.3.3
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 7th March 1996
 *
 *
 *
 * Introduction:
 *
 * This file contains all the additional code required to get the Amiga
 * port of the Atari800 emulator up and running.
 *
 * See "Atari800.guide" for info
 */



#include <clib/asl_protos.h>
#include <clib/exec_protos.h>
#include <devices/gameport.h>
#include <devices/inputevent.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <stdio.h>
#include <string.h>
#include <workbench/workbench.h>

#include "atari.h"
#include "colours.h"

#define GAMEPORT0 0
#define GAMEPORT1 1

#define OCS_ChipSet 0
#define ECS_ChipSet 1
#define AGA_ChipSet 2

#ifdef DICE_C
static struct IntuitionBase *IntuitionBase = NULL;
static struct GfxBase *GfxBase = NULL;
static struct LayersBase *LayersBase = NULL;
static struct Library *AslBase = NULL;
#endif

#ifdef GNU_C
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct LayersBase *LayersBase = NULL;
struct Library *AslBase = NULL;
#endif

extern int countdown_rate;
extern int refresh_rate;

static struct Window *WindowMain = NULL;
static struct Window *WindowAbout = NULL;
static struct Window *WindowNotSupported = NULL;
static struct Window *WindowSetup = NULL;
static struct Window *WindowController = NULL;
static struct Window *WindowYesNo = NULL;
static struct Window *WindowIconified = NULL;

int old_stick_0 = 0x1f;
int old_stick_1 = 0x1f;

static int consol;
static int trig0;
static int stick0;

struct InputEvent gameport_data;
struct MsgPort *gameport_msg_port;
struct IOStdReq *gameport_io_msg;
BOOL gameport_error;

static UBYTE *image_data;

static UWORD ScreenType;
static struct Screen *ScreenMain = NULL;
static struct NewScreen NewScreen;
static struct Image image_Screen;
static int ScreenID;
static int ScreenWidth;
static int ScreenHeight;
static int ScreenDepth;
static int TotalColours;

struct AppWindow *AppWindow = NULL;
struct AppMenuItem *AppMenuItem = NULL;
struct AppIcon *AppIcon = NULL;
struct DiskObject *AppDiskObject = NULL;
struct MsgPort *AppMessagePort = NULL;
struct AppMessage *AppMessage = NULL;

static struct Menu *menu_Project = NULL;
static struct MenuItem *menu_Project00 = NULL;
static struct MenuItem *menu_Project01 = NULL;
static struct MenuItem *menu_Project02 = NULL;
static struct MenuItem *menu_Project03 = NULL;

static struct Menu *menu_System = NULL;
static struct MenuItem *menu_System00 = NULL;
static struct MenuItem *menu_System01 = NULL;
static struct MenuItem *menu_System01s00 = NULL;
static struct MenuItem *menu_System01s01 = NULL;
static struct MenuItem *menu_System01s02 = NULL;
static struct MenuItem *menu_System01s03 = NULL;
static struct MenuItem *menu_System01s04 = NULL;
static struct MenuItem *menu_System01s05 = NULL;
static struct MenuItem *menu_System01s06 = NULL;
static struct MenuItem *menu_System01s07 = NULL;
static struct MenuItem *menu_System02 = NULL;
static struct MenuItem *menu_System02s00 = NULL;
static struct MenuItem *menu_System02s01 = NULL;
static struct MenuItem *menu_System02s02 = NULL;
static struct MenuItem *menu_System02s03 = NULL;
static struct MenuItem *menu_System02s04 = NULL;
static struct MenuItem *menu_System02s05 = NULL;
static struct MenuItem *menu_System02s06 = NULL;
static struct MenuItem *menu_System02s07 = NULL;
static struct MenuItem *menu_System03 = NULL;
static struct MenuItem *menu_System03s00 = NULL;
static struct MenuItem *menu_System03s01 = NULL;
static struct MenuItem *menu_System03s02 = NULL;
static struct MenuItem *menu_System04 = NULL;
static struct MenuItem *menu_System05 = NULL;
static struct MenuItem *menu_System06 = NULL;
static struct MenuItem *menu_System07 = NULL;
static struct MenuItem *menu_System08 = NULL;
static struct MenuItem *menu_System09 = NULL;

static struct Menu *menu_Console = NULL;
static struct MenuItem *menu_Console00 = NULL;
static struct MenuItem *menu_Console01 = NULL;
static struct MenuItem *menu_Console02 = NULL;
static struct MenuItem *menu_Console03 = NULL;
static struct MenuItem *menu_Console04 = NULL;
static struct MenuItem *menu_Console05 = NULL;
static struct MenuItem *menu_Console06 = NULL;

static struct Menu *menu_Prefs = NULL;
static struct MenuItem *menu_Prefs00 = NULL;
static struct MenuItem *menu_Prefs00s00 = NULL;
static struct MenuItem *menu_Prefs00s01 = NULL;
static struct MenuItem *menu_Prefs00s02 = NULL;
static struct MenuItem *menu_Prefs00s03 = NULL;
static struct MenuItem *menu_Prefs01 = NULL;
static struct MenuItem *menu_Prefs01s00 = NULL;
static struct MenuItem *menu_Prefs01s01 = NULL;
static struct MenuItem *menu_Prefs02 = NULL;
static struct MenuItem *menu_Prefs02s00 = NULL;
static struct MenuItem *menu_Prefs02s01 = NULL;
static struct MenuItem *menu_Prefs02s02 = NULL;
static struct MenuItem *menu_Prefs02s03 = NULL;
static struct MenuItem *menu_Prefs03 = NULL;
static struct MenuItem *menu_Prefs03s00 = NULL;
static struct MenuItem *menu_Prefs03s01 = NULL;
static struct MenuItem *menu_Prefs03s02 = NULL;
static struct MenuItem *menu_Prefs03s03 = NULL;

struct MemHeader MemHeader;

static UWORD *data_Screen;

static ChipSet;
static ColourEnabled;
static Controller;
static CustomScreen;

static int PaddlePos;

static UpdatePalette;

static void DisplayAboutWindow();
static void DisplayNotSupportedWindow();
static void DisplaySetupWindow();
static void DisplayControllerWindow();
static int DisplayYesNoWindow();
static int InsertDisk();
static struct Gadget *MakeGadget();
static struct MenuItem *MakeMenuItem();
static struct Menu *MakeMenu();
static void Rule();
static void ShowText();

static int gui_GridWidth = 4;
static int gui_GridHeight = 7;
static int gui_WindowOffsetLeftEdge = 0;	/* 4 */
static int gui_WindowOffsetTopEdge = 0;		/* 11 */

static int LibraryVersion;

#define gadget_Null 0
#define gadget_Button 1
#define gadget_Check 2
#define gadget_String 3
#define gadget_Mutual 4

#define controller_Null 0
#define controller_Joystick 1
#define controller_Paddle 2



/*
 * ====================
 * Define data_Button64
 * ====================
 */

UWORD data_Button64[] =
{
	0x0000, 0x0000, 0x0000, 0x0001,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x7FFF, 0xFFFF, 0xFFFF, 0xFFFF,

	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFE,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0x8000, 0x0000, 0x0000, 0x0000
};



/*
 * ============================
 * Define data_Button64Selected
 * ============================
 */

UWORD data_Button64Selected[] =
{
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFE,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0xC000, 0x0000, 0x0000, 0x0000,
	0x8000, 0x0000, 0x0000, 0x0000,

	0x0000, 0x0000, 0x0000, 0x0001,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x0000, 0x0000, 0x0000, 0x0003,
	0x7FFF, 0xFFFF, 0xFFFF, 0xFFFF
};



/*
 * ========================
 * Define data_MutualGadget
 * ========================
 */

UWORD data_MutualGadget[] =
{
	0x0000,
	0x0000,
	0x0004,
	0x0006,
	0x0006,
	0x0003,
	0x0003,
	0x0003,
	0x0003,
	0x0006,
	0x0006,
	0x181C,
	0x1FF8,
	0x07E0,

	0x07E0,
	0x1FF8,
	0x3818,
	0x6000,
	0x6000,
	0xC000,
	0xC000,
	0xC000,
	0xC000,
	0x6000,
	0x6000,
	0x2000,
	0x0000,
	0x0000
};



/*
 * ================================
 * Define data_MutualGadgetSelected
 * ================================
 */

UWORD data_MutualGadgetSelected[] =
{
	0x07E0,
	0x1FF8,
	0x3818,
	0x6180,
	0x67E0,
	0xC7E0,
	0xCFF0,
	0xCFF0,
	0xC7E0,
	0x67E0,
	0x6180,
	0x2000,
	0x0000,
	0x0000,

	0x0000,
	0x0000,
	0x0004,
	0x0006,
	0x0006,
	0x0003,
	0x0003,
	0x0003,
	0x0003,
	0x0006,
	0x0006,
	0x181C,
	0x1FF8,
	0x07E0
};



struct Image image_Button64;
struct Image image_Button64Selected;
struct Image image_MutualGadget;
struct Image image_MutualGadgetSelected;

UWORD *chip_Button64 = NULL;
UWORD *chip_Button64Selected = NULL;
UWORD *chip_MutualGadget = NULL;
UWORD *chip_MutualGadgetSelected = NULL;

/*
 * ================
 * Atari_Initialize
 * ================
 */

/*
 * Revision     : v0.2.1
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 12th October 1995
 */

void Atari_Initialise(int *argc, char *argv[])
{
	struct NewWindow NewWindow;

	char *ptr;
	int i;
	int j;
	struct IntuiMessage *IntuiMessage;

	ULONG Class;
	USHORT Code;
	APTR Address;

	int QuitRoutine;

	/*
	 * =========
	 * Zoom Data
	 * =========
	 */

	WORD Zoomdata[4] =
	{
		0, 11, 200, 15,
	};

	/*



	 */

	chip_Button64 = (UWORD *) AllocMem(sizeof(data_Button64), MEMF_CHIP);
	chip_Button64Selected = (UWORD *) AllocMem(sizeof(data_Button64Selected), MEMF_CHIP);
	chip_MutualGadget = (UWORD *) AllocMem(sizeof(data_MutualGadget), MEMF_CHIP);
	chip_MutualGadgetSelected = (UWORD *) AllocMem(sizeof(data_MutualGadgetSelected), MEMF_CHIP);

	memcpy(chip_Button64, data_Button64, sizeof(data_Button64));
	memcpy(chip_Button64Selected, data_Button64Selected, sizeof(data_Button64Selected));
	memcpy(chip_MutualGadget, data_MutualGadget, sizeof(data_MutualGadget));
	memcpy(chip_MutualGadgetSelected, data_MutualGadgetSelected, sizeof(data_MutualGadgetSelected));

	/*



	 */

	image_Button64.LeftEdge = 0;
	image_Button64.TopEdge = 0;
	image_Button64.Width = 64;
	image_Button64.Height = 14;
	image_Button64.Depth = 2;
	image_Button64.ImageData = chip_Button64;
	image_Button64.PlanePick = 3;
	image_Button64.PlaneOnOff = (UBYTE) NULL;
	image_Button64.NextImage = NULL;

	image_Button64Selected.LeftEdge = 0;
	image_Button64Selected.TopEdge = 0;
	image_Button64Selected.Width = 64;
	image_Button64Selected.Height = 14;
	image_Button64Selected.Depth = 2;
	image_Button64Selected.ImageData = chip_Button64Selected;
	image_Button64Selected.PlanePick = 3;
	image_Button64Selected.PlaneOnOff = (UBYTE) NULL;
	image_Button64Selected.NextImage = NULL;

	image_MutualGadget.LeftEdge = 0;
	image_MutualGadget.TopEdge = 0;
	image_MutualGadget.Width = 16;
	image_MutualGadget.Height = 14;
	image_MutualGadget.Depth = 2;
	image_MutualGadget.ImageData = chip_MutualGadget;
	image_MutualGadget.PlanePick = 3;
	image_MutualGadget.PlaneOnOff = (UBYTE) NULL;
	image_MutualGadget.NextImage = NULL;

	image_MutualGadgetSelected.LeftEdge = 0;
	image_MutualGadgetSelected.TopEdge = 0;
	image_MutualGadgetSelected.Width = 16;
	image_MutualGadgetSelected.Height = 14;
	image_MutualGadgetSelected.Depth = 2;
	image_MutualGadgetSelected.ImageData = chip_MutualGadgetSelected;
	image_MutualGadgetSelected.PlanePick = 3;
	image_MutualGadgetSelected.PlaneOnOff = (UBYTE) NULL;
	image_MutualGadgetSelected.NextImage = NULL;

	/*
	 * =======================
	 * Process startup options
	 * =======================
	 */

	ChipSet = AGA_ChipSet;
	ColourEnabled = TRUE;
	UpdatePalette = TRUE;
	CustomScreen = TRUE;
	Controller = controller_Joystick;
	PaddlePos = 228;
	LibraryVersion = 39;

	for (i = j = 1; i < *argc; i++) {
/*
   printf("%d: %s\n", i,argv[i]);
 */

		if (strcmp(argv[i], "-ocs") == 0) {
			printf("Requested OCS graphics mode... Accepted!\n");
			ChipSet = OCS_ChipSet;
			LibraryVersion = 37;
		}
		else if (strcmp(argv[i], "-ecs") == 0) {
			printf("Requested ECS graphics mode... Accepted!\n");
			ChipSet = ECS_ChipSet;
			LibraryVersion = 37;
		}
		else if (strcmp(argv[i], "-aga") == 0) {
			printf("Requested AGA graphics mode... Accepted!\n");
			printf("  Enabled custom screen automatically\n");
			ChipSet = AGA_ChipSet;
			CustomScreen = TRUE;
			LibraryVersion = 39;
		}
		else if (strcmp(argv[i], "-grey") == 0) {
			printf("Requested grey scale display... Accepted!\n");
			ColourEnabled = FALSE;
		}
		else if (strcmp(argv[i], "-colour") == 0) {
			printf("Requested colour display... Accepted!\n");
			ColourEnabled = TRUE;
		}
		else if (strcmp(argv[i], "-wb") == 0) {
			printf("Requested Workbench window... Accepted!\n");
			CustomScreen = FALSE;
			LibraryVersion = 37;
			if (ChipSet == AGA_ChipSet) {
				printf("  ECS chip set automatically enabled\n");
				ChipSet = ECS_ChipSet;
			}
		}
		else if (strcmp(argv[i], "-joystick") == 0) {
			printf("Requested joystick controller... Accepted!\n");
			Controller = controller_Joystick;
		}
		else if (strcmp(argv[i], "-paddle") == 0) {
			printf("Requested paddle controller... Accepted!\n");
			Controller = controller_Paddle;
		}
		else {
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	IntuitionBase = (struct IntuitionBase *) OpenLibrary("intuition.library", LibraryVersion);
	if (!IntuitionBase) {
		printf("Failed to open intuition.library\n");
		Atari_Exit(0);
	}
	GfxBase = (struct GfxBase *) OpenLibrary("graphics.library", LibraryVersion);
	if (!GfxBase) {
		printf("Failed to open graphics.library\n");
		Atari_Exit(0);
	}
	LayersBase = (struct LayersBase *) OpenLibrary("layers.library", LibraryVersion);
	if (!LayersBase) {
		printf("Failed to open layers.library\n");
		Atari_Exit(0);
	}
	AslBase = (struct Library *) OpenLibrary("asl.library", LibraryVersion);
	if (!AslBase) {
		printf("Failed to open asl.library\n");
		Atari_Exit(0);
	}
	data_Screen = (UWORD *) AllocMem(46080 * 2, MEMF_CHIP);
	if (!data_Screen) {
		printf("Oh BUGGER!\n");
	}
	printf("data_Screen = %x\n", data_Screen);

	/*
	 * ==============
	 * Setup Joystick
	 * ==============
	 */

	gameport_msg_port = (struct MsgPort *) CreatePort(0, 0);
	if (!gameport_msg_port) {
		printf("Failed to create gameport_msg_port\n");
		Atari_Exit(0);
	}
	gameport_io_msg = (struct IOStdReq *) CreateStdIO(gameport_msg_port);
	if (!gameport_io_msg) {
		printf("Failed to create gameport_io_msg\n");
		Atari_Exit(0);
	}
	gameport_error = OpenDevice("gameport.device", GAMEPORT1, gameport_io_msg, 0xFFFF);
	if (gameport_error) {
		printf("Failed to open the gameport.device\n");
		Atari_Exit(0);
	} {
		BYTE type = 0;

		gameport_io_msg->io_Command = GPD_ASKCTYPE;
		gameport_io_msg->io_Length = 1;
		gameport_io_msg->io_Data = (APTR) & type;

		DoIO(gameport_io_msg);

		if (type) {
			printf("gameport already in use\n");
			gameport_error = TRUE;
		}
	}

	{
		BYTE type = GPCT_ABSJOYSTICK;

		gameport_io_msg->io_Command = GPD_SETCTYPE;
		gameport_io_msg->io_Length = 1;
		gameport_io_msg->io_Data = (APTR) & type;

		DoIO(gameport_io_msg);

		if (gameport_io_msg->io_Error) {
			printf("Failed to set controller type\n");
		}
	}

	{
		struct GamePortTrigger gpt;

		gpt.gpt_Keys = GPTF_DOWNKEYS | GPTF_UPKEYS;
		gpt.gpt_Timeout = 0;
		gpt.gpt_XDelta = 1;
		gpt.gpt_YDelta = 1;

		gameport_io_msg->io_Command = GPD_SETTRIGGER;
		gameport_io_msg->io_Length = sizeof(gpt);
		gameport_io_msg->io_Data = (APTR) & gpt;

		DoIO(gameport_io_msg);

		if (gameport_io_msg->io_Error) {
			printf("Failed to set controller trigger\n");
		}
	}

	{
		struct InputEvent *Data;

		gameport_io_msg->io_Command = GPD_READEVENT;
		gameport_io_msg->io_Length = sizeof(struct InputEvent);
		gameport_io_msg->io_Data = (APTR) & gameport_data;
		gameport_io_msg->io_Flags = 0;
	}

	SendIO(gameport_io_msg);

	ScreenType = WBENCHSCREEN;

	if (ChipSet == AGA_ChipSet) {
		DisplaySetupWindow();
	}
	SetupDisplay();

	DisplayAboutWindow();

/*
 * Setup Project Menu
 */

	menu_Project00 = MakeMenuItem(0, 0, 96, 10, "Iconify", NULL, NULL);
	menu_Project01 = MakeMenuItem(0, 15, 96, 10, "Help", 'H', NULL);
	menu_Project02 = MakeMenuItem(0, 30, 96, 10, "About", '?', NULL);
	menu_Project03 = MakeMenuItem(0, 45, 96, 10, "Quit", 'N', NULL);

	menu_Project00->NextItem = menu_Project01;
	menu_Project01->NextItem = menu_Project02;
	menu_Project02->NextItem = menu_Project03;

	menu_Project = MakeMenu(0, 0, 48, "Amiga", menu_Project00);

/*
 * Setup Disk Menu
 */

	menu_System00 = MakeMenuItem(0, 0, 144, 10, "Boot disk", NULL, NULL);
	menu_System01 = MakeMenuItem(0, 10, 144, 10, "Insert disk      �", NULL, NULL);
	menu_System01s00 = MakeMenuItem(128, 0, 80, 10, "Drive 1...", NULL, NULL);
	menu_System01s01 = MakeMenuItem(128, 10, 80, 10, "Drive 2...", NULL, NULL);
	menu_System01s02 = MakeMenuItem(128, 20, 80, 10, "Drive 3...", NULL, NULL);
	menu_System01s03 = MakeMenuItem(128, 30, 80, 10, "Drive 4...", NULL, NULL);
	menu_System01s04 = MakeMenuItem(128, 40, 80, 10, "Drive 5...", NULL, NULL);
	menu_System01s05 = MakeMenuItem(128, 50, 80, 10, "Drive 6...", NULL, NULL);
	menu_System01s06 = MakeMenuItem(128, 60, 80, 10, "Drive 7...", NULL, NULL);
	menu_System01s07 = MakeMenuItem(128, 70, 80, 10, "Drive 8...", NULL, NULL);

	menu_System01s00->NextItem = menu_System01s01;
	menu_System01s01->NextItem = menu_System01s02;
	menu_System01s02->NextItem = menu_System01s03;
	menu_System01s03->NextItem = menu_System01s04;
	menu_System01s04->NextItem = menu_System01s05;
	menu_System01s05->NextItem = menu_System01s06;
	menu_System01s06->NextItem = menu_System01s07;

	menu_System01->SubItem = menu_System01s00;

	menu_System02 = MakeMenuItem(0, 20, 144, 10, "Eject disk       �", NULL, NULL);
	menu_System02s00 = MakeMenuItem(128, 0, 56, 10, "Drive 1", NULL, NULL);
	menu_System02s01 = MakeMenuItem(128, 10, 56, 10, "Drive 2", NULL, NULL);
	menu_System02s02 = MakeMenuItem(128, 20, 56, 10, "Drive 3", NULL, NULL);
	menu_System02s03 = MakeMenuItem(128, 30, 56, 10, "Drive 4", NULL, NULL);
	menu_System02s04 = MakeMenuItem(128, 40, 56, 10, "Drive 5", NULL, NULL);
	menu_System02s05 = MakeMenuItem(128, 50, 56, 10, "Drive 6", NULL, NULL);
	menu_System02s06 = MakeMenuItem(128, 60, 56, 10, "Drive 7", NULL, NULL);
	menu_System02s07 = MakeMenuItem(128, 70, 56, 10, "Drive 8", NULL, NULL);

	menu_System02s00->NextItem = menu_System02s01;
	menu_System02s01->NextItem = menu_System02s02;
	menu_System02s02->NextItem = menu_System02s03;
	menu_System02s03->NextItem = menu_System02s04;
	menu_System02s04->NextItem = menu_System02s05;
	menu_System02s05->NextItem = menu_System02s06;
	menu_System02s06->NextItem = menu_System02s07;

	menu_System02->SubItem = menu_System02s00;

	menu_System03 = MakeMenuItem(0, 35, 144, 10, "Insert Cartridge �", NULL, NULL);
	menu_System03s00 = MakeMenuItem(128, 0, 128, 10, "8K Cart...", NULL, NULL);
	menu_System03s01 = MakeMenuItem(128, 10, 128, 10, "16K Cart...", NULL, NULL);
	menu_System03s02 = MakeMenuItem(128, 20, 128, 10, "OSS SuperCart...", NULL, NULL);

	menu_System03s00->NextItem = menu_System03s01;
	menu_System03s01->NextItem = menu_System03s02;

	menu_System03->SubItem = menu_System03s00;

	menu_System04 = MakeMenuItem(0, 45, 144, 10, "Remove Cartridge", NULL, NULL);
	menu_System05 = MakeMenuItem(0, 60, 144, 10, "Enable PIL", NULL, NULL);
	menu_System06 = MakeMenuItem(0, 75, 144, 10, "Atari 800 OS/A", NULL, NULL);
	menu_System07 = MakeMenuItem(0, 85, 144, 10, "Atari 800 OS/B", NULL, NULL);
	menu_System08 = MakeMenuItem(0, 95, 144, 10, "Atari 800XL", NULL, NULL);
	menu_System09 = MakeMenuItem(0, 105, 144, 10, "Atari 130XE", NULL, NULL);

	menu_System00->NextItem = menu_System01;
	menu_System01->NextItem = menu_System02;
	menu_System02->NextItem = menu_System03;
	menu_System03->NextItem = menu_System04;
	menu_System04->NextItem = menu_System05;
	menu_System05->NextItem = menu_System06;
	menu_System06->NextItem = menu_System07;
	menu_System07->NextItem = menu_System08;
	menu_System08->NextItem = menu_System09;

	menu_System = MakeMenu(48, 0, 56, "System", menu_System00);

/*
 * Setup Console Menu
 */

	menu_Console00 = MakeMenuItem(0, 0, 80, 10, "Option", NULL, NULL);
	menu_Console01 = MakeMenuItem(0, 10, 80, 10, "Select", NULL, NULL);
	menu_Console02 = MakeMenuItem(0, 20, 80, 10, "Start", NULL, NULL);
	menu_Console03 = MakeMenuItem(0, 30, 80, 10, "Help", NULL, NULL);

	if (machine == Atari) {
		menu_Console03->Flags = ITEMTEXT | HIGHCOMP;
	}
	menu_Console04 = MakeMenuItem(0, 45, 80, 10, "Break", NULL, NULL);
	menu_Console05 = MakeMenuItem(0, 60, 80, 10, "Reset", NULL, NULL);
	menu_Console06 = MakeMenuItem(0, 75, 80, 10, "Cold Start", NULL, NULL);

	menu_Console00->NextItem = menu_Console01;
	menu_Console01->NextItem = menu_Console02;
	menu_Console02->NextItem = menu_Console03;
	menu_Console03->NextItem = menu_Console04;
	menu_Console04->NextItem = menu_Console05;
	menu_Console05->NextItem = menu_Console06;

	menu_Console = MakeMenu(104, 0, 64, "Console", menu_Console00);

/*
 * Setup Prefs Menu
 */

/*
   menu_Prefs00 = MakeMenuItem (0, 0, 96, 10, "Controller �", NULL, NULL);
   menu_Prefs00s00 = MakeMenuItem (80, 0, 80, 10, "  Joystick", NULL, 2);
   menu_Prefs00s01 = MakeMenuItem (80, 10, 80, 10, "  Paddle", NULL, 1);

   if (Controller == controller_Joystick)
   {
   menu_Prefs00s00->Flags = menu_Prefs00s00->Flags | CHECKED;
   }
   else if (Controller == controller_Paddle)
   {
   menu_Prefs00s01->Flags = menu_Prefs00s01->Flags | CHECKED;
   }

   menu_Prefs00s00->NextItem = menu_Prefs00s01;

   menu_Prefs00->SubItem = menu_Prefs00s00;
 */

	menu_Prefs00 = MakeMenuItem(0, 0, 96, 10, "Controller �", NULL, NULL);
	menu_Prefs00s00 = MakeMenuItem(80, 0, 80, 10, "Port 1...", NULL, NULL);
	menu_Prefs00s01 = MakeMenuItem(80, 10, 80, 10, "Port 2...", NULL, NULL);
	menu_Prefs00s01->Flags = ITEMTEXT | HIGHCOMP;

	menu_Prefs00s02 = MakeMenuItem(80, 20, 80, 10, "Port 3...", NULL, NULL);
/*
   if (machine != Atari)
   {
   menu_Prefs00s02->Flags = ITEMTEXT | HIGHCOMP;
   }
 */
	menu_Prefs00s02->Flags = ITEMTEXT | HIGHCOMP;

	menu_Prefs00s03 = MakeMenuItem(80, 30, 80, 10, "Port 4...", NULL, NULL);
/*
   if (machine != Atari)
   {
   menu_Prefs00s03->Flags = ITEMTEXT | HIGHCOMP;
   }
 */
	menu_Prefs00s03->Flags = ITEMTEXT | HIGHCOMP;

	menu_Prefs00s00->NextItem = menu_Prefs00s01;
	menu_Prefs00s01->NextItem = menu_Prefs00s02;
	menu_Prefs00s02->NextItem = menu_Prefs00s03;

	menu_Prefs00->SubItem = menu_Prefs00s00;

	menu_Prefs01 = MakeMenuItem(0, 10, 96, 10, "Display    �", NULL, NULL);
	menu_Prefs01s00 = MakeMenuItem(80, 0, 64, 10, "  Colour", NULL, 2);
	menu_Prefs01s01 = MakeMenuItem(80, 10, 64, 10, "  Grey", NULL, 1);

	if (ColourEnabled) {
		menu_Prefs01s00->Flags = menu_Prefs01s00->Flags | CHECKED;
	}
	else {
		menu_Prefs01s01->Flags = menu_Prefs01s01->Flags | CHECKED;
	}

	menu_Prefs01s00->NextItem = menu_Prefs01s01;

	menu_Prefs01->SubItem = menu_Prefs01s00;

	menu_Prefs02 = MakeMenuItem(0, 20, 96, 10, "Refresh    �", NULL, NULL);
	menu_Prefs02s00 = MakeMenuItem(80, 0, 24, 10, "  1", NULL, 14);
	menu_Prefs02s01 = MakeMenuItem(80, 10, 24, 10, "  2", NULL, 13);
	menu_Prefs02s02 = MakeMenuItem(80, 20, 24, 10, "  4", NULL, 11);
	menu_Prefs02s03 = MakeMenuItem(80, 30, 24, 10, "  8", NULL, 7);

	if (refresh_rate == 1) {
		menu_Prefs02s00->Flags = menu_Prefs02s00->Flags | CHECKED;
	}
	else if (refresh_rate == 2) {
		menu_Prefs02s01->Flags = menu_Prefs02s01->Flags | CHECKED;
	}
	else if (refresh_rate == 4) {
		menu_Prefs02s02->Flags = menu_Prefs02s02->Flags | CHECKED;
	}
	else if (refresh_rate == 8) {
		menu_Prefs02s03->Flags = menu_Prefs02s03->Flags | CHECKED;
	}
	menu_Prefs02s00->NextItem = menu_Prefs02s01;
	menu_Prefs02s01->NextItem = menu_Prefs02s02;
	menu_Prefs02s02->NextItem = menu_Prefs02s03;

	menu_Prefs02->SubItem = menu_Prefs02s00;

	menu_Prefs03 = MakeMenuItem(0, 30, 96, 10, "Countdown  �", NULL, NULL);
	menu_Prefs03s00 = MakeMenuItem(80, 0, 56, 10, "  4000", NULL, 14);
	menu_Prefs03s01 = MakeMenuItem(80, 10, 56, 10, "  8000", NULL, 13);
	menu_Prefs03s02 = MakeMenuItem(80, 20, 56, 10, "  16000", NULL, 11);
	menu_Prefs03s03 = MakeMenuItem(80, 30, 56, 10, "  32000", NULL, 7);

	if (countdown_rate == 4000) {
		menu_Prefs03s00->Flags = menu_Prefs03s00->Flags | CHECKED;
	}
	else if (countdown_rate == 8000) {
		menu_Prefs03s01->Flags = menu_Prefs03s01->Flags | CHECKED;
	}
	else if (countdown_rate == 16000) {
		menu_Prefs03s02->Flags = menu_Prefs03s02->Flags | CHECKED;
	}
	else if (countdown_rate == 32000) {
		menu_Prefs03s03->Flags = menu_Prefs03s03->Flags | CHECKED;
	}
	menu_Prefs03s00->NextItem = menu_Prefs03s01;
	menu_Prefs03s01->NextItem = menu_Prefs03s02;
	menu_Prefs03s02->NextItem = menu_Prefs03s03;

	menu_Prefs03->SubItem = menu_Prefs03s00;

	menu_Prefs00->NextItem = menu_Prefs01;
	menu_Prefs01->NextItem = menu_Prefs02;
	menu_Prefs02->NextItem = menu_Prefs03;

	menu_Prefs = MakeMenu(168, 0, 48, "Prefs", menu_Prefs00);

/*
 * Link Menus
 */

	menu_Project->NextMenu = menu_System;
	menu_System->NextMenu = menu_Console;
	menu_Console->NextMenu = menu_Prefs;

	SetMenuStrip(WindowMain, menu_Project);

	image_Screen.LeftEdge = 0;
	image_Screen.TopEdge = 0;
	image_Screen.Width = 384;
	image_Screen.Height = 240;
	image_Screen.Depth = 8;
	image_Screen.ImageData = data_Screen;
	image_Screen.PlanePick = 255;
	image_Screen.PlaneOnOff = NULL;
	image_Screen.NextImage = NULL;

	/*
	 * ============================
	 * Storage for Atari 800 Screen
	 * ============================
	 */

	image_data = (UBYTE *) malloc(ATARI_WIDTH * ATARI_HEIGHT);
	if (!image_data) {
		printf("Failed to allocate space for image\n");
		Atari_Exit(0);
	}
	for (ptr = image_data, i = 0; i < ATARI_WIDTH * ATARI_HEIGHT; i++, ptr++)
		*ptr = 0;
	for (ptr = (char *) data_Screen, i = 0; i < ATARI_WIDTH * ATARI_HEIGHT; i++, ptr++)
		*ptr = 0;

	trig0 = 1;
	stick0 = 15;
	consol = 7;
}



/*
 * ==========
 * Atari_Exit
 * ==========
 */

/*
 * Revision     : v0.2.0
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 7th September 1995
 */

int Atari_Exit(int run_monitor)
{
	if (run_monitor) {
		if (monitor()) {
			return TRUE;
		}
	}
	AbortIO(gameport_io_msg);

	while (GetMsg(gameport_msg_port)) {
	}

	if (!gameport_error) {
		{
			BYTE type = GPCT_NOCONTROLLER;

			gameport_io_msg->io_Command = GPD_SETCTYPE;
			gameport_io_msg->io_Length = 1;
			gameport_io_msg->io_Data = (APTR) & type;

			DoIO(gameport_io_msg);

			if (gameport_io_msg->io_Error) {
				printf("Failed to set controller type\n");
			}
		}

		CloseDevice(gameport_io_msg);
	}
	FreeMem(data_Screen, 46080 * 2);

	if (gameport_io_msg) {
		DeleteStdIO(gameport_io_msg);
	}
	if (gameport_msg_port) {
		DeletePort(gameport_msg_port);
	}
	if (WindowMain) {
		CloseWindow(WindowMain);
	}
	if (ScreenMain) {
		CloseScreen(ScreenMain);
	}
	if (LayersBase) {
		CloseLibrary(LayersBase);
	}
	if (GfxBase) {
		CloseLibrary(GfxBase);
	}
	if (IntuitionBase) {
		CloseLibrary(IntuitionBase);
	}
	exit(1);

/*
   return FALSE;
 */
}



/*
 * ===================
 * Atari_DisplayScreen
 * ===================
 */

/*
 * Revision     : v0.2.0a
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 7th September 1995
 */

void Atari_DisplayScreen(UBYTE * screen)
{
/*
   UWORD    *bitplane0;
   UWORD    *bitplane1;
   UWORD    *bitplane2;
   UWORD    *bitplane3;
   UWORD    *bitplane4;

   bitplane0 = WindowMain->RPort->BitMap->Planes[0];
   word0 = *bitplane0;

   bitplane1 = WindowMain->RPort->BitMap->Planes[1]; 
   word1 = *bitplane1;

   bitplane2 = WindowMain->RPort->BitMap->Planes[2];
   word2 = *bitplane2;

   bitplane3 = WindowMain->RPort->BitMap->Planes[3];
   word3 = *bitplane3;

   bitplane4 = WindowMain->RPort->BitMap->Planes[4];
   word4 = *bitplane4;
 */

	if (ChipSet == AGA_ChipSet) {
		ULONG *bitplane0;

		bitplane0 = &data_Screen[0];

		if (ColourEnabled)
			amiga_aga_colour(screen, bitplane0);
		else
			amiga_aga_bw(screen, bitplane0);
	}
	else {
		ULONG *bitplane0;
		ULONG *bitplane1;
		ULONG *bitplane2;
		ULONG *bitplane3;
		ULONG *bitplane4;

		ULONG word0, word1, word2, word3, word4;

		int ypos;
		int xpos;
		int tbit = 31;
		UBYTE *scanline_ptr;

		BYTE pens[256];
		int pen;

		scanline_ptr = image_data;

		bitplane0 = &data_Screen[0];
		word0 = *bitplane0;

		bitplane1 = &data_Screen[5760];
		word1 = *bitplane1;

		bitplane2 = &data_Screen[11520];
		word2 = *bitplane2;

		bitplane3 = &data_Screen[17280];
		word3 = *bitplane3;

		bitplane4 = &data_Screen[23040];
		word4 = *bitplane4;

		for (pen = 0; pen < 256; pen++) {
			pens[pen] = 0;
		}

		pen = 5;

		if (CustomScreen) {
			pen = 1;
		}
		if (ColourEnabled) {
			for (ypos = 0; ypos < ATARI_HEIGHT; ypos++) {
				for (xpos = 0; xpos < ATARI_WIDTH; xpos++) {
					UBYTE colour;

					colour = *screen;

					if (pens[colour] == 0) {
						if (pen < 33) {
							int rgb = colortable[colour];
							int red;
							int green;
							int blue;

							pens[colour] = pen;
							pen++;

							if (UpdatePalette) {
								red = (rgb & 0x00ff0000) >> 20;
								green = (rgb & 0x0000ff00) >> 12;
								blue = (rgb & 0x000000ff) >> 4;

								SetRGB4(&WindowMain->WScreen->ViewPort, pen - 1, red, green, blue);
							}
						}
					}
					colour = pens[*screen++];

					if (colour != *scanline_ptr) {
						ULONG mask;

						mask = ~(1 << tbit);

						word0 = (word0 & mask) | (((colour) & 1) << tbit);
						word1 = (word1 & mask) | (((colour >> 1) & 1) << tbit);
						word2 = (word2 & mask) | (((colour >> 2) & 1) << tbit);
						word3 = (word3 & mask) | (((colour >> 3) & 1) << tbit);
						word4 = (word4 & mask) | (((colour >> 4) & 1) << tbit);

						*scanline_ptr++ = colour;
					}
					else {
						scanline_ptr++;
					}

					if (--tbit == -1) {
						*bitplane0++ = word0;
						word0 = *bitplane0;

						*bitplane1++ = word1;
						word1 = *bitplane1;

						*bitplane2++ = word2;
						word2 = *bitplane2;

						*bitplane3++ = word3;
						word3 = *bitplane3;

						*bitplane4++ = word4;
						word4 = *bitplane4;

						tbit = 31;
					}
				}
			}
		}
		else {
			for (ypos = 0; ypos < ATARI_HEIGHT; ypos++) {
				for (xpos = 0; xpos < ATARI_WIDTH; xpos++) {
					UBYTE colour;

					colour = (*screen++ & 0x0f) + pen - 1;

					if (colour != *scanline_ptr) {
						ULONG mask;

						mask = ~(1 << tbit);

						word0 = (word0 & mask) | (((colour) & 1) << tbit);
						word1 = (word1 & mask) | (((colour >> 1) & 1) << tbit);
						word2 = (word2 & mask) | (((colour >> 2) & 1) << tbit);
						word3 = (word3 & mask) | (((colour >> 3) & 1) << tbit);
						word4 = (word4 & mask) | (((colour >> 4) & 1) << tbit);

						*scanline_ptr++ = colour;
					}
					else {
						scanline_ptr++;
					}

					if (--tbit == -1) {
						*bitplane0++ = word0;
						word0 = *bitplane0;

						*bitplane1++ = word1;
						word1 = *bitplane1;

						*bitplane2++ = word2;
						word2 = *bitplane2;

						*bitplane3++ = word3;
						word3 = *bitplane3;

						*bitplane4++ = word4;
						word4 = *bitplane4;

						tbit = 31;
					}
				}
			}
		}
	}

	if (CustomScreen) {
		DrawImage(WindowMain->RPort, &image_Screen, -32, 0);
	}
	else {
		DrawImage(WindowMain->RPort, &image_Screen, 0, 0);
	}
}



/*
 * ==============
 * Atari_Keyboard
 * ==============
 */

/*
 * Revision     : v0.3.3
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 7th March 1996
 *
 * Notes: Currently contains GUI monitoring code as well. At some time in
 * the future I intend on removing this from this code.
 */

int Atari_Keyboard(void)
{
	int keycode;

	struct IntuiMessage *IntuiMessage;

	ULONG Class;
	USHORT Code;
	APTR Address;
	SHORT MouseX;
	SHORT MouseY;

	USHORT MenuItem = NULL;

	int Menu = NULL;
	int Item = NULL;
	int SubItem = NULL;

	keycode = -1;

	while (IntuiMessage = (struct IntuiMessage *) GetMsg(WindowMain->UserPort)) {
		Class = IntuiMessage->Class;
		Code = IntuiMessage->Code;
		Address = IntuiMessage->IAddress;
		MouseX = IntuiMessage->MouseX;
		MouseY = IntuiMessage->MouseY;

		MenuItem = Code;

		Menu = MENUNUM(MenuItem);
		Item = ITEMNUM(MenuItem);
		SubItem = SUBNUM(MenuItem);

		if (Class == MENUVERIFY) {
			if (ChipSet == OCS_ChipSet || ChipSet == ECS_ChipSet) {
				if (CustomScreen) {
					int i;

					UpdatePalette = FALSE;

					for (i = 0; i < 16; i++) {
						int rgb = colortable[i];
						int red;
						int green;
						int blue;

						red = (rgb & 0x00ff0000) >> 20;
						green = (rgb & 0x0000ff00) >> 12;
						blue = (rgb & 0x000000ff) >> 4;

						SetRGB4(&WindowMain->WScreen->ViewPort, i, red, green, blue);
					}

					SetRGB4(&WindowMain->WScreen->ViewPort, 24, 6, 8, 11);
				}
			}
		}
		ReplyMsg(IntuiMessage);

		switch (Class) {
		case VANILLAKEY:
			keycode = Code;

			switch (keycode) {
			case 0x01:
				keycode = AKEY_CTRL_a;
				break;
			case 0x02:
				keycode = AKEY_CTRL_b;
				break;
			case 0x03:
				keycode = AKEY_CTRL_c;
				break;
			case 0x04:
				keycode = AKEY_CTRL_d;
				break;
			case 0x05:
				keycode = AKEY_CTRL_E;
				break;
			case 0x06:
				keycode = AKEY_CTRL_F;
				break;
			case 0x07:
				keycode = AKEY_CTRL_G;
				break;
/*
   case 0x08 :
   keycode = AKEY_CTRL_H;
   break;
 */
/* TAB - see case '\t' :
   case 0x09 :
   keycode = AKEY_CTRL_I;
   break;
 */
			case 0x0a:
				keycode = AKEY_CTRL_J;
				break;
			case 0x0b:
				keycode = AKEY_CTRL_K;
				break;
			case 0x0c:
				keycode = AKEY_CTRL_L;
				break;
/*
   case 0x0d :
   keycode = AKEY_CTRL_M;
   break;
 */
			case 0x0e:
				keycode = AKEY_CTRL_N;
				break;
			case 0x0f:
				keycode = AKEY_CTRL_O;
				break;
			case 0x10:
				keycode = AKEY_CTRL_P;
				break;
			case 0x11:
				keycode = AKEY_CTRL_Q;
				break;
			case 0x12:
				keycode = AKEY_CTRL_R;
				break;
			case 0x13:
				keycode = AKEY_CTRL_S;
				break;
			case 0x14:
				keycode = AKEY_CTRL_T;
				break;
			case 0x15:
				keycode = AKEY_CTRL_U;
				break;
			case 0x16:
				keycode = AKEY_CTRL_V;
				break;
			case 0x17:
				keycode = AKEY_CTRL_W;
				break;
			case 0x18:
				keycode = AKEY_CTRL_X;
				break;
			case 0x19:
				keycode = AKEY_CTRL_Y;
				break;
			case 0x1a:
				keycode = AKEY_CTRL_Z;
				break;
			case 8:
				keycode = AKEY_BACKSPACE;
				break;
			case 13:
				keycode = AKEY_RETURN;
				break;
			case 0x1b:
				keycode = AKEY_ESCAPE;
				break;
			case '0':
				keycode = AKEY_0;
				break;
			case '1':
				keycode = AKEY_1;
				break;
			case '2':
				keycode = AKEY_2;
				break;
			case '3':
				keycode = AKEY_3;
				break;
			case '4':
				keycode = AKEY_4;
				break;
			case '5':
				keycode = AKEY_5;
				break;
			case '6':
				keycode = AKEY_6;
				break;
			case '7':
				keycode = AKEY_7;
				break;
			case '8':
				keycode = AKEY_8;
				break;
			case '9':
				keycode = AKEY_9;
				break;
			case 'A':
			case 'a':
				keycode = AKEY_a;
				break;
			case 'B':
			case 'b':
				keycode = AKEY_b;
				break;
			case 'C':
			case 'c':
				keycode = AKEY_c;
				break;
			case 'D':
			case 'd':
				keycode = AKEY_d;
				break;
			case 'E':
			case 'e':
				keycode = AKEY_e;
				break;
			case 'F':
			case 'f':
				keycode = AKEY_f;
				break;
			case 'G':
			case 'g':
				keycode = AKEY_g;
				break;
			case 'H':
			case 'h':
				keycode = AKEY_h;
				break;
			case 'I':
			case 'i':
				keycode = AKEY_i;
				break;
			case 'J':
			case 'j':
				keycode = AKEY_j;
				break;
			case 'K':
			case 'k':
				keycode = AKEY_k;
				break;
			case 'L':
			case 'l':
				keycode = AKEY_l;
				break;
			case 'M':
			case 'm':
				keycode = AKEY_m;
				break;
			case 'N':
			case 'n':
				keycode = AKEY_n;
				break;
			case 'O':
			case 'o':
				keycode = AKEY_o;
				break;
			case 'P':
			case 'p':
				keycode = AKEY_p;
				break;
			case 'Q':
			case 'q':
				keycode = AKEY_q;
				break;
			case 'R':
			case 'r':
				keycode = AKEY_r;
				break;
			case 'S':
			case 's':
				keycode = AKEY_s;
				break;
			case 'T':
			case 't':
				keycode = AKEY_t;
				break;
			case 'U':
			case 'u':
				keycode = AKEY_u;
				break;
			case 'V':
			case 'v':
				keycode = AKEY_v;
				break;
			case 'W':
			case 'w':
				keycode = AKEY_w;
				break;
			case 'X':
			case 'x':
				keycode = AKEY_x;
				break;
			case 'Y':
			case 'y':
				keycode = AKEY_y;
				break;
			case 'Z':
			case 'z':
				keycode = AKEY_z;
				break;
			case ' ':
				keycode = AKEY_SPACE;
				break;
			case '\t':
				keycode = AKEY_TAB;
				break;
			case '!':
				keycode = AKEY_EXCLAMATION;
				break;
			case '"':
				keycode = AKEY_DBLQUOTE;
				break;
			case '#':
				keycode = AKEY_HASH;
				break;
			case '$':
				keycode = AKEY_DOLLAR;
				break;
			case '%':
				keycode = AKEY_PERCENT;
				break;
			case '&':
				keycode = AKEY_AMPERSAND;
				break;
			case '\'':
				keycode = AKEY_QUOTE;
				break;
			case '@':
				keycode = AKEY_AT;
				break;
			case '(':
				keycode = AKEY_PARENLEFT;
				break;
			case ')':
				keycode = AKEY_PARENRIGHT;
				break;
			case '<':
				keycode = AKEY_LESS;
				break;
			case '>':
				keycode = AKEY_GREATER;
				break;
			case '=':
				keycode = AKEY_EQUAL;
				break;
			case '?':
				keycode = AKEY_QUESTION;
				break;
			case '-':
				keycode = AKEY_MINUS;
				break;
			case '+':
				keycode = AKEY_PLUS;
				break;
			case '*':
				keycode = AKEY_ASTERISK;
				break;
			case '/':
				keycode = AKEY_SLASH;
				break;
			case ':':
				keycode = AKEY_COLON;
				break;
			case ';':
				keycode = AKEY_SEMICOLON;
				break;
			case ',':
				keycode = AKEY_COMMA;
				break;
			case '.':
				keycode = AKEY_FULLSTOP;
				break;
			case '_':
				keycode = AKEY_UNDERSCORE;
				break;
			case '[':
				keycode = AKEY_BRACKETLEFT;
				break;
			case ']':
				keycode = AKEY_BRACKETRIGHT;
				break;
			case '^':
				keycode = AKEY_CIRCUMFLEX;
				break;
			case '\\':
				keycode = AKEY_BACKSLASH;
				break;
			case '|':
				keycode = AKEY_BAR;
				break;
			default:
				keycode = AKEY_NONE;
				break;
			}
			break;
		case RAWKEY:
			switch (Code) {
			case 0x51:
				consol &= 0x03;
				keycode = AKEY_NONE;
				break;
			case 0x52:
				consol &= 0x05;
				keycode = AKEY_NONE;
				break;
			case 0x53:
				consol &= 0x06;
				keycode = AKEY_NONE;
				break;
			case 0x55:
				keycode = AKEY_PIL;
				break;
			case 0x56:
				keycode = AKEY_BREAK;
				break;
			case 0x59:
				keycode = AKEY_NONE;
				break;
			case 0x4f:
				keycode = AKEY_LEFT;
				break;
			case 0x4c:
				keycode = AKEY_UP;
				break;
			case 0x4e:
				keycode = AKEY_RIGHT;
				break;
			case 0x4d:
				keycode = AKEY_DOWN;
				break;
			default:
				break;
			}
			break;
		case MOUSEBUTTONS:
			if (Controller == controller_Paddle) {
				switch (Code) {
				case SELECTDOWN:
					stick0 = 251;
					break;
				case SELECTUP:
					stick0 = 255;
					break;
				default:
					break;
				}
			}
			break;
		case MOUSEMOVE:
			if (Controller == controller_Paddle) {
				if (MouseX > 57) {
					if (MouseX < 287) {
						PaddlePos = 228 - (MouseX - 58);
					}
					else {
						PaddlePos = 0;
					}
				}
				else {
					PaddlePos = 228;
				}
			}
			break;
		case CLOSEWINDOW:
			if (DisplayYesNoWindow()) {
				keycode = AKEY_EXIT;
			}
			break;
		case MENUPICK:
			switch (Menu) {
			case 0:
				switch (Item) {
				case 0:
					Iconify();
/*
   DisplayNotSupportedWindow ();
 */
					break;
				case 1:
					SystemTags
						(
							"Run >Nil: <Nil: MultiView Atari800.guide",
							SYS_Input, NULL,
							SYS_Output, NULL,
							TAG_DONE
						);
					break;
				case 2:
					DisplayAboutWindow();
					break;
				case 3:
					if (DisplayYesNoWindow()) {
						keycode = AKEY_EXIT;
					}
					break;
				default:
					break;
				}
				break;
			case 1:
				switch (Item) {
				case 0:
					if (InsertDisk(1)) {
						keycode = AKEY_COLDSTART;
					}
					break;
				case 1:
					switch (SubItem) {
					case 0:
						if (InsertDisk(1)) {
						}
						break;
					case 1:
						if (InsertDisk(2)) {
						}
						break;
					case 2:
						if (InsertDisk(3)) {
						}
						break;
					case 3:
						if (InsertDisk(4)) {
						}
						break;
					case 4:
						if (InsertDisk(5)) {
						}
						break;
					case 5:
						if (InsertDisk(6)) {
						}
						break;
					case 6:
						if (InsertDisk(7)) {
						}
						break;
					case 7:
						if (InsertDisk(8)) {
						}
						break;
					default:
						break;
					}
					break;
				case 2:
					switch (SubItem) {
					case 0:
						SIO_Dismount(1);
						break;
					case 1:
						SIO_Dismount(2);
						break;
					case 2:
						SIO_Dismount(3);
						break;
					case 3:
						SIO_Dismount(4);
						break;
					case 4:
						SIO_Dismount(5);
						break;
					case 5:
						SIO_Dismount(6);
						break;
					case 6:
						SIO_Dismount(7);
						break;
					case 7:
						SIO_Dismount(8);
						break;
					default:
						break;
					}
					break;
				case 3:
					switch (SubItem) {
					case 0:
						InsertROM(0);
						break;
					case 1:
						InsertROM(1);
						break;
					case 2:
						InsertROM(2);
						break;
					default:
						break;
					}
					break;
				case 4:
					Remove_ROM();
					Coldstart();
					break;
				case 5:
					EnablePILL();
					Coldstart();
					break;
				case 6:
					/*
					 * Atari800 OS/A
					 */

					{
						int status;

						status = Initialise_AtariOSA();
						if (status) {
							if (machine == Atari) {
								menu_Console03->Flags = ITEMTEXT | HIGHCOMP;
							}
							else {
								menu_Console03->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
							}
						}
						else {
						}
					}
					break;
				case 7:
					/*
					 * Atari800 OS/B
					 */

					{
						int status;
						status = Initialise_AtariOSB();
						if (status) {
							if (machine == Atari) {
								menu_Console03->Flags = ITEMTEXT | HIGHCOMP;
							}
							else {
								menu_Console03->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
							}
						}
						else {
						}
					}
					break;
				case 8:
					/*
					 * Atari800 XL
					 */

					{
						int status;

						status = Initialise_AtariXL();
						if (status) {
							if (machine == Atari) {
								menu_Console03->Flags = ITEMTEXT | HIGHCOMP;
							}
							else {
								menu_Console03->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
							}
						}
						else {
						}
					}
					break;
				case 9:
					/*
					 * Atari XE
					 */

					{
						int status;

						status = Initialise_AtariXE();
						if (status) {
							if (machine == Atari) {
								menu_Console03->Flags = ITEMTEXT | HIGHCOMP;
							}
							else {
								menu_Console03->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
							}
						}
						else {
						}
					}
					break;
				default:
					break;
				}
				break;
			case 2:
				switch (Item) {
				case 0:
					consol &= 0x03;
					keycode = AKEY_NONE;
					break;
				case 1:
					consol &= 0x05;
					keycode = AKEY_NONE;
					break;
				case 2:
					consol &= 0x06;
					keycode = AKEY_NONE;
					break;
				case 3:
					keycode = AKEY_HELP;
					break;
				case 4:
					keycode = AKEY_BREAK;
					break;
				case 5:
					if (DisplayYesNoWindow()) {
						keycode = AKEY_WARMSTART;
					}
					break;
				case 6:
					if (DisplayYesNoWindow()) {
						keycode = AKEY_COLDSTART;
					}
					break;
				default:
					break;
				}
				break;
			case 3:
				switch (Item) {
				case 0:
					switch (SubItem) {
					case 0:
						DisplayControllerWindow(0);
						break;
					case 1:
						DisplayControllerWindow(1);
						break;
					case 2:
						DisplayControllerWindow(2);
						break;
					case 3:
						DisplayControllerWindow(3);
						break;
					default:
						break;
					}
					break;
				case 1:
					switch (SubItem) {
					case 0:
						ColourEnabled = TRUE;
						break;
					case 1:
						{
							int i;

							ColourEnabled = FALSE;

							for (i = 0; i < 16; i++) {
								int rgb = colortable[i];
								int red;
								int green;
								int blue;

								red = (rgb & 0x00ff0000) >> 20;
								green = (rgb & 0x0000ff00) >> 12;
								blue = (rgb & 0x000000ff) >> 4;

								if (CustomScreen) {
									SetRGB4(&WindowMain->WScreen->ViewPort, i, red, green, blue);
								}
								else {
									SetRGB4(&WindowMain->WScreen->ViewPort, i + 4, red, green, blue);
								}
							}
						}
						break;
					default:
						break;
					}
					break;
				case 2:
					switch (SubItem) {
					case 0:
						refresh_rate = 1;
						break;
					case 1:
						refresh_rate = 2;
						break;
					case 2:
						refresh_rate = 4;
						break;
					case 3:
						refresh_rate = 8;
						break;
					default:
						break;
					}
					break;
				case 3:
					switch (SubItem) {
					case 0:
						countdown_rate = 4000;
						break;
					case 1:
						countdown_rate = 8000;
						break;
					case 2:
						countdown_rate = 16000;
						break;
					case 3:
						countdown_rate = 32000;
						break;
					default:
						break;
					}
					break;
				default:
					break;
				}
			default:
				break;
			}

			UpdatePalette = TRUE;

			break;
		default:
			break;
		}
	}

	return keycode;
}



/*
 * ==============
 * Atari_Joystick
 * ==============
 */

/*
 * Revision     : v0.1.9a
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 2nd September 1995
 */

int Atari_Joystick()
{
	WORD x;
	WORD y;
	UWORD code;

	int stick = 0;

	if (GetMsg(gameport_msg_port)) {
		x = gameport_data.ie_X;
		y = gameport_data.ie_Y;
		code = gameport_data.ie_Code;

		switch (x) {
		case -1:
			switch (y) {
			case -1:
				stick = 10;
				break;
			case 0:
				stick = 11;
				break;
			case 1:
				stick = 9;
				break;
			default:
				break;
			}
			break;
		case 0:
			switch (y) {
			case -1:
				stick = 14;
				break;
			case 0:
				stick = 15;
				break;
			case 1:
				stick = 13;
				break;
			default:
				break;
			}
			break;
		case 1:
			switch (y) {
			case -1:
				stick = 6;
				break;
			case 0:
				stick = 7;
				break;
			case 1:
				stick = 5;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		if (code == IECODE_LBUTTON) {
			if (stick == 0) {
				stick = old_stick_0 & 15;
			}
			else {
				stick = stick & 15;
			}
		}
		if (code == IECODE_LBUTTON + IECODE_UP_PREFIX) {
			if (stick == 0) {
				stick = old_stick_0 | 16;
			}
			else {
				stick = stick | 16;
			}
		}
		old_stick_0 = stick;
		SendIO(gameport_io_msg);
	}
	if (stick == 0) {
		stick = old_stick_0;
	}
	stick0 = stick & 0x0f;
	trig0 = (stick & 0x10) >> 4;

/*
   printf ("stick0 = %d   trig0 = %d\n", stick0, trig0);
   return stick;
 */
}



/*
 * ==========
 * Atari_PORT
 * ==========
 */

/*
 * Revision     : v0.2.0a
 * Introduced   : 2nd September 1995
 * Last updated : 3rd September 1995
 *
 * Notes: Hook required by v0.2.0. All the work is actually done in
 * Atari_Joystick and Atari_Paddle.
 */

int Atari_PORT(int num)
{
	if (num == 0) {
		if (Controller == controller_Joystick) {
			Atari_Joystick();
			return 0xf0 | stick0;
		}
		else {
			return stick0;
		}
	}
	else {
		return 0xff;
	}
}



/*
 * ==========
 * Atari_TRIG
 * ==========
 */

/*
 * Revision     : v0.1.9a
 * Introduced   : 2nd September 1995
 * Last updated : 2nd September 1995
 *
 * Notes: Hook required by v0.1.9. All the work is actually done in
 * Atari_Joystick.
 */

int Atari_TRIG(int num)
{
	if (num == 0) {
		Atari_Joystick();
		return trig0;
	}
	else
		return 1;
}



/*
 * =========
 * Atari_POT
 * =========
 */

/*
 * Revision     : v0.2.0a
 * Introduced   : 2nd September 1995
 * Last updated : 3rd September 1995
 *
 * Notes: Currently acts as nothing more than a hook.
 */

int Atari_POT(int num)
{
	return PaddlePos;
}



/*
 * ============
 * Atari_CONSOL
 * ============
 */

/*
 * Revision     : v0.1.9a
 * Introduced   : 2nd September 1995
 * Last updated : 2nd September 1995
 *
 * Notes: Hook required by v0.1.9. All the work is actually done in
 * Atari_Keyboard.
 */

int Atari_CONSOL(void)
{
	return consol;
}

/*
 * ==================
 * DisplayAboutWindow
 * ==================
 */

/*
 * Revision     : v0.3.3
 * Introduced   : 2nd September 1995
 * Last updated : 7th March 1996
 */

void DisplayAboutWindow(void)
{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow NewWindow;

	struct Gadget *Gadget1 = NULL;

	int QuitRoutine;

	NewWindow.LeftEdge = 12;
	NewWindow.TopEdge = 46;
	NewWindow.Width = 296;
	NewWindow.Height = 148;
	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 15;
	NewWindow.IDCMPFlags = CLOSEWINDOW | GADGETUP;
	NewWindow.Flags = WINDOWCLOSE | GIMMEZEROZERO | WINDOWDRAG | SMART_REFRESH | ACTIVATE;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Title = ATARI_TITLE;
	NewWindow.Screen = ScreenMain;
	NewWindow.Type = ScreenType;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 92;
	NewWindow.MinHeight = 65;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowAbout = (struct Window *) OpenWindowTags
		(
			&NewWindow,
			WA_NewLookMenus, TRUE,
			WA_MenuHelp, TRUE,
			WA_ScreenTitle, ATARI_TITLE,
			TAG_DONE
		);

	if (ScreenType == CUSTOMSCREEN) {
		SetRast(WindowAbout->RPort, 6);
	}
	Rule(0, 15, 288, WindowAbout);

	{
		char temp[128];
		char *ptr;

		strcpy(temp, ATARI_TITLE);
		ptr = strchr(temp, ',');
		if (ptr) {
			int title_len;
			int version_len;

			*ptr++ = '\0';
			ptr++;

			title_len = strlen(temp);
			version_len = strlen(ptr);

			ShowText((36 - title_len), 1, WindowAbout, temp);
			ShowText((36 - version_len), 4, WindowAbout, ptr);
		}
		else {
			ShowText(19, 1, WindowAbout, "Atari800 Emulator");
			ShowText(21, 4, WindowAbout, "Version Unknown");
		}

		ShowText(17, 7, WindowAbout, "Original program by");
		ShowText(25, 9, WindowAbout, "David Firth");
		ShowText(18, 12, WindowAbout, "Amiga Module V8 by");
		ShowText(20, 14, WindowAbout, "Stephen A. Firth");
	}

	Gadget1 = MakeGadget(55, 17, 64, 14, 0, WindowAbout, "Ok", gadget_Button);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE) {
		while (IntuiMessage = (struct IntuiMessage *) GetMsg(WindowAbout->UserPort)) {
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg(IntuiMessage);

			switch (Class) {
			case CLOSEWINDOW:
				QuitRoutine = TRUE;
				break;
			case GADGETUP:
				if (Address == (APTR) Gadget1) {
					QuitRoutine = TRUE;
				}
				break;
			default:
				break;
			}
		}
	}

	CloseWindow(WindowAbout);
}



/*
 * ==================
 * DisplaySetupWindow
 * ==================
 */

/*
 * Revision     : v0.2.1
 * Introduced   : 3rd September 1995
 * Last updated : 12th October 1995
 */

void DisplaySetupWindow(void)
{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow NewWindow;

	struct Gadget *Gadget1 = NULL;
	struct Gadget *Gadget2 = NULL;
	struct Gadget *Gadget3 = NULL;
	struct Gadget *Gadget4 = NULL;
	struct Gadget *Gadget5 = NULL;
	struct Gadget *Gadget6 = NULL;
	struct Gadget *Gadget7 = NULL;

	int QuitRoutine;

	NewWindow.LeftEdge = 0;
	NewWindow.TopEdge = 11;
	NewWindow.Width = 296;
	NewWindow.Height = 148;		/* 177 */
	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 15;
	NewWindow.IDCMPFlags = CLOSEWINDOW | GADGETUP;
	NewWindow.Flags = WINDOWCLOSE | GIMMEZEROZERO | WINDOWDRAG | SMART_REFRESH | ACTIVATE;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Title = ATARI_TITLE;
	NewWindow.Screen = NULL;
	NewWindow.Type = WBENCHSCREEN;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 92;
	NewWindow.MinHeight = 65;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowSetup = (struct Window *) OpenWindowTags
		(
			&NewWindow,
			WA_NewLookMenus, TRUE,
			WA_MenuHelp, TRUE,
			WA_ScreenTitle, ATARI_TITLE,
			TAG_DONE
		);

	Rule(0, 15, 288, WindowSetup);

	ShowText(1, 1, WindowSetup, "Display:");

/*
   ShowText (17, 1, WindowSetup, "CLI Startup Options");

   ShowText (2, 4, WindowSetup, "-ocs      OCS emulation module");
   ShowText (2, 6, WindowSetup, "-ecs      ECS emulation module");
   ShowText (2, 8, WindowSetup, "-aga      AGA emulation module");

   ShowText (2, 11, WindowSetup, "-wb       Open window on Workbench");
   ShowText (2, 13, WindowSetup, "          (forces ECS emulation)");

   ShowText (2, 16, WindowSetup, "-colour   Colour display");
   ShowText (2, 18, WindowSetup, "-grey     Grey scale display");
 */

/*
   Gadget1 = MakeGadget (0, 21, 64, 14, 0, WindowSetup, "AGA Screen", gadget_Button);
   Gadget2 = MakeGadget (24, 21, 64, 14, 0, WindowSetup, "ECS Screen", gadget_Button);
   Gadget3 = MakeGadget (48, 21, 64, 14, 0, WindowSetup, "OCS Screen", gadget_Button);
   Gadget4 = MakeGadget (0, 23, 64, 14, 0, WindowSetup, "AGA Window", gadget_Button);
   Gadget5 = MakeGadget (24, 23, 64, 14, 0, WindowSetup, "ECS Window", gadget_Button);
   Gadget6 = MakeGadget (48, 23, 64, 14, 0, WindowSetup, "OCS Window", gadget_Button);
 */
	Gadget1 = MakeGadget(3, 3, 16, 14, 0, WindowSetup, "AGA Screen", gadget_Mutual);
	Gadget1->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
	Gadget2 = MakeGadget(3, 5, 16, 14, 0, WindowSetup, "ECS Screen", gadget_Mutual);
	Gadget3 = MakeGadget(3, 7, 16, 14, 0, WindowSetup, "OCS Screen", gadget_Mutual);
	Gadget4 = MakeGadget(3, 9, 16, 14, 0, WindowSetup, "AGA Window", gadget_Mutual);
	Gadget5 = MakeGadget(3, 11, 16, 14, 0, WindowSetup, "ECS Window", gadget_Mutual);
	Gadget6 = MakeGadget(3, 13, 16, 14, 0, WindowSetup, "OCS Window", gadget_Mutual);
	Gadget7 = MakeGadget(55, 17, 64, 14, 0, WindowSetup, "Ok", gadget_Button);

	RefreshGadgets(Gadget1, WindowSetup, NULL);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE) {
		while (IntuiMessage = (struct IntuiMessage *) GetMsg(WindowSetup->UserPort)) {
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg(IntuiMessage);

			switch (Class) {
			case CLOSEWINDOW:
				QuitRoutine = TRUE;
				break;
			case GADGETUP:
				{
					int NewFlagsForGadget1 = GADGIMAGE | GADGHIMAGE;
					int NewFlagsForGadget2 = GADGIMAGE | GADGHIMAGE;
					int NewFlagsForGadget3 = GADGIMAGE | GADGHIMAGE;
					int NewFlagsForGadget4 = GADGIMAGE | GADGHIMAGE;
					int NewFlagsForGadget5 = GADGIMAGE | GADGHIMAGE;
					int NewFlagsForGadget6 = GADGIMAGE | GADGHIMAGE;
					int NewFlags = FALSE;

					if (Address == (APTR) Gadget1) {
						ChipSet = AGA_ChipSet;
						CustomScreen = TRUE;
						NewFlagsForGadget1 = GADGIMAGE | GADGHIMAGE | SELECTED;
						NewFlags = TRUE;
					}
					else if (Address == (APTR) Gadget2) {
						ChipSet = ECS_ChipSet;
						CustomScreen = TRUE;
						NewFlagsForGadget2 = GADGIMAGE | GADGHIMAGE | SELECTED;
						NewFlags = TRUE;
					}
					else if (Address == (APTR) Gadget3) {
						ChipSet = OCS_ChipSet;
						CustomScreen = TRUE;
						NewFlagsForGadget3 = GADGIMAGE | GADGHIMAGE | SELECTED;
						NewFlags = TRUE;
					}
					else if (Address == (APTR) Gadget4) {
						/*
						 * NOTE: AGA Window Modes Currently Not Supported
						 */

						DisplayNotSupportedWindow();

						Gadget4->Flags = GADGIMAGE | GADGHIMAGE;
						RefreshGadgets(Gadget1, WindowSetup, NULL);
/*
   QuitRoutine = TRUE;
   ChipSet = ECS_ChipSet;
   CustomScreen = FALSE;
 */
					}
					else if (Address == (APTR) Gadget5) {
						ChipSet = ECS_ChipSet;
						CustomScreen = FALSE;
						NewFlagsForGadget5 = GADGIMAGE | GADGHIMAGE | SELECTED;
						NewFlags = TRUE;
					}
					else if (Address == (APTR) Gadget6) {
						ChipSet = OCS_ChipSet;
						CustomScreen = FALSE;
						NewFlagsForGadget6 = GADGIMAGE | GADGHIMAGE | SELECTED;
						NewFlags = TRUE;
					}
					else if (Address == (APTR) Gadget7) {
						QuitRoutine = TRUE;
					}
					if (NewFlags) {
						Gadget1->Flags = NewFlagsForGadget1;
						Gadget2->Flags = NewFlagsForGadget2;
						Gadget3->Flags = NewFlagsForGadget3;
						Gadget4->Flags = NewFlagsForGadget4;
						Gadget5->Flags = NewFlagsForGadget5;
						Gadget6->Flags = NewFlagsForGadget6;
						RefreshGadgets(Gadget1, WindowSetup, NULL);
					}
				}
				break;
			default:
				break;
			}
		}
	}

	CloseWindow(WindowSetup);
}



/*
 * =======================
 * DisplayControllerWindow
 * =======================
 */

/*
 * Revision     : v0.2.1a
 * Introduced   : 3rd October 1995
 * Last updated : 12th October 1995
 */

void DisplayControllerWindow(int Port)
{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow NewWindow;

	struct Gadget *Gadget1 = NULL;
	struct Gadget *Gadget2 = NULL;
	struct Gadget *Gadget3 = NULL;
	struct Gadget *Gadget4 = NULL;
	struct Gadget *Gadget5 = NULL;
	struct Gadget *Gadget6 = NULL;
	struct Gadget *Gadget7 = NULL;
	struct Gadget *Gadget8 = NULL;
	struct Gadget *Gadget9 = NULL;

	int QuitRoutine;
	int Accepted;
	int NewController;

	NewWindow.LeftEdge = 0;
	NewWindow.TopEdge = 11;
	NewWindow.Width = 296;
/* Re-insert for v.0.2.2
   NewWindow.Height = 155;
 */
	NewWindow.Height = 92;		/* 177 */
	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 15;
	NewWindow.IDCMPFlags = CLOSEWINDOW | GADGETUP;
	NewWindow.Flags = WINDOWCLOSE | GIMMEZEROZERO | WINDOWDRAG | SMART_REFRESH | ACTIVATE;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Title = "Controller Options";
	NewWindow.Screen = ScreenMain;
	NewWindow.Type = ScreenType;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 92;
	NewWindow.MinHeight = 65;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowController = (struct Window *) OpenWindowTags
		(
			&NewWindow,
			WA_NewLookMenus, TRUE,
			WA_MenuHelp, TRUE,
			WA_ScreenTitle, ATARI_TITLE,
			TAG_DONE
		);

	if (ScreenType == CUSTOMSCREEN) {
		SetRast(WindowController->RPort, 6);
	}
/* Re-insert for v0.2.2
   Rule (0, 16, 288, WindowController);
 */
	Rule(0, 7, 288, WindowController);

	ShowText(1, 1, WindowController, "Emulate:");

	Gadget1 = MakeGadget(3, 3, 16, 14, 0, WindowController, "Joystick", gadget_Mutual);

	if (Controller == controller_Joystick) {
		Gadget1->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
	}
	Gadget2 = MakeGadget(3, 5, 16, 14, 0, WindowController, "Paddle", gadget_Mutual);

	if (Controller == controller_Paddle) {
		Gadget2->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
	}
/* Re-insert for v0.2.2
   ShowText (36, 1, WindowController, "Via:");

   Gadget3 = MakeGadget (38, 3, 16, 14, 0, WindowController, "Joystick", gadget_Mutual);
   Gadget4 = MakeGadget (38, 5, 16, 14, 0, WindowController, "Mouse", gadget_Mutual);

   ShowText (1, 8, WindowController, "Amiga port:");

   Gadget5 = MakeGadget (3, 10, 16, 14, 0, WindowController, "0", gadget_Mutual);
   Gadget6 = MakeGadget (3, 12, 16, 14, 0, WindowController, "1", gadget_Mutual);
   Gadget7 = MakeGadget (3, 14, 16, 14, 0, WindowController, "None", gadget_Mutual);

   Gadget8 = MakeGadget (39, 18, 64, 14, 0, WindowController, "Ok", gadget_Button);
   Gadget9 = MakeGadget (55, 18, 64, 14, 0, WindowController, "Cancel", gadget_Button);
 */

	Gadget8 = MakeGadget(39, 9, 64, 14, 0, WindowController, "Ok", gadget_Button);
	Gadget9 = MakeGadget(55, 9, 64, 14, 0, WindowController, "Cancel", gadget_Button);

	RefreshGadgets(Gadget1, WindowController, NULL);

	QuitRoutine = FALSE;
	Accepted = FALSE;

	NewController = Controller;

	while (QuitRoutine == FALSE) {
		while (IntuiMessage = (struct IntuiMessage *) GetMsg(WindowController->UserPort)) {
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg(IntuiMessage);

			switch (Class) {
			case CLOSEWINDOW:
				QuitRoutine = TRUE;
				break;
			case GADGETUP:
				if (Address == (APTR) Gadget1) {
					Gadget1->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
					Gadget2->Flags = GADGIMAGE | GADGHIMAGE;
					RefreshGadgets(Gadget1, WindowController, NULL);
					NewController = controller_Joystick;
				}
				else if (Address == (APTR) Gadget2) {
					Gadget1->Flags = GADGIMAGE | GADGHIMAGE;
					Gadget2->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
					RefreshGadgets(Gadget1, WindowController, NULL);
					NewController = controller_Paddle;
				}
/* Re-insert for v0.2.2
   else if (Address == (APTR) Gadget3)
   {
   }
   else if (Address == (APTR) Gadget4)
   {
   }
   else if (Address == (APTR) Gadget5)
   {
   }
   else if (Address == (APTR) Gadget6)
   {
   }
   else if (Address == (APTR) Gadget7)
   {
   }
 */
				else if (Address == (APTR) Gadget8) {
					QuitRoutine = TRUE;
					Accepted = TRUE;
				}
				else if (Address == (APTR) Gadget9) {
					QuitRoutine = TRUE;
				}
				break;
			default:
				break;
			}
		}
	}

	if (Accepted) {
		Controller = NewController;
	}
	CloseWindow(WindowController);
}



/*
 * =========================
 * DisplayNotSupportedWindow
 * =========================
 */

/*
 * Revision     : v0.2.0
 * Introduced   : 3rd September 1995
 * Last updated : 9th September 1995
 */

void DisplayNotSupportedWindow(void)
{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow NewWindow;

	struct Gadget *Gadget1 = NULL;

	int QuitRoutine;

	NewWindow.LeftEdge = 12;
	NewWindow.TopEdge = 91;
	NewWindow.Width = 296;
	NewWindow.Height = 57;
	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 15;
	NewWindow.IDCMPFlags = CLOSEWINDOW | GADGETUP;
	NewWindow.Flags = WINDOWCLOSE | GIMMEZEROZERO | WINDOWDRAG | SMART_REFRESH | ACTIVATE;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Title = "Warning!";
	NewWindow.Screen = ScreenMain;
	NewWindow.Type = ScreenType;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 92;
	NewWindow.MinHeight = 65;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowNotSupported = (struct Window *) OpenWindowTags
		(
			&NewWindow,
			WA_NewLookMenus, TRUE,
			WA_MenuHelp, TRUE,
			WA_ScreenTitle, ATARI_TITLE,
			TAG_DONE
		);

	if (ScreenType == CUSTOMSCREEN) {
		SetRast(WindowNotSupported->RPort, 6);
	}
	Rule(0, 2, 288, WindowNotSupported);

	ShowText(15, 1, WindowNotSupported, "Feature not supported");

	Gadget1 = MakeGadget(55, 4, 64, 14, 0, WindowNotSupported, "Ok", gadget_Button);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE) {
		while (IntuiMessage = (struct IntuiMessage *) GetMsg(WindowNotSupported->UserPort)) {
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg(IntuiMessage);

			switch (Class) {
			case CLOSEWINDOW:
				QuitRoutine = TRUE;
				break;
			case GADGETUP:
				if (Address == (APTR) Gadget1) {
					QuitRoutine = TRUE;
				}
				break;
			default:
				break;
			}
		}
	}

	CloseWindow(WindowNotSupported);
}



/*
 * ==================
 * DisplayYesNoWindow
 * ==================
 */

/*
 * Revision     : v0.2.0a
 * Introduced   : 9th September 1995
 * Last updated : 9th September 1995
 */

int DisplayYesNoWindow(void)
{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow NewWindow;

	struct Gadget *Gadget1 = NULL;
	struct Gadget *Gadget2 = NULL;

	int QuitRoutine;
	int Answer;

	Answer = FALSE;

	NewWindow.LeftEdge = 12;
	NewWindow.TopEdge = 91;
	NewWindow.Width = 296;
	NewWindow.Height = 57;
	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 15;
	NewWindow.IDCMPFlags = CLOSEWINDOW | GADGETUP;
	NewWindow.Flags = WINDOWCLOSE | GIMMEZEROZERO | WINDOWDRAG | SMART_REFRESH | ACTIVATE;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Title = "Warning!";
	NewWindow.Screen = ScreenMain;
	NewWindow.Type = ScreenType;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 92;
	NewWindow.MinHeight = 65;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowYesNo = (struct Window *) OpenWindowTags
		(
			&NewWindow,
			WA_NewLookMenus, TRUE,
			WA_MenuHelp, TRUE,
			WA_ScreenTitle, ATARI_TITLE,
			TAG_DONE
		);

	if (ScreenType == CUSTOMSCREEN) {
		SetRast(WindowYesNo->RPort, 6);
	}
	Rule(0, 2, 288, WindowYesNo);

	ShowText(1, 1, WindowYesNo, "Are you sure?");

	Gadget1 = MakeGadget(39, 4, 64, 14, 0, WindowYesNo, "Ok", gadget_Button);
	Gadget2 = MakeGadget(55, 4, 64, 14, 0, WindowYesNo, "Cancel", gadget_Button);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE) {
		while (IntuiMessage = (struct IntuiMessage *) GetMsg(WindowYesNo->UserPort)) {
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg(IntuiMessage);

			switch (Class) {
			case CLOSEWINDOW:
				QuitRoutine = TRUE;
				break;
			case GADGETUP:
				if (Address == (APTR) Gadget1) {
					QuitRoutine = TRUE;
					Answer = TRUE;
				}
				else if (Address == (APTR) Gadget2) {
					QuitRoutine = TRUE;
				}
				break;
			default:
				break;
			}
		}
	}

	CloseWindow(WindowYesNo);

	return Answer;
}



/*
 * ============
 * MakeMenuItem
 * ============
 */

/*
 * Revision     : v0.2.0
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 9th September 1995
 *
 * Notes: A simple routine to help GUI development.
 */

struct MenuItem *MakeMenuItem(int LeftEdge, int TopEdge, int Width,
						   int Height, char *Title, int Key, int Exclude)
{
	struct IntuiText *IntuiText;
	struct MenuItem *MenuItem;

	IntuiText = (struct IntuiText *) malloc(sizeof(struct IntuiText));
	if (!IntuiText) {
	}
	IntuiText->FrontPen = 1;
	IntuiText->BackPen = 2;
	IntuiText->DrawMode = JAM1;
	IntuiText->LeftEdge = NULL;
	IntuiText->TopEdge = 1;
	IntuiText->ITextFont = NULL;
	IntuiText->IText = (UBYTE *) Title;
	IntuiText->NextText = NULL;

	MenuItem = (struct MenuItem *) malloc(sizeof(struct MenuItem));
	if (!MenuItem) {
	}
	MenuItem->NextItem = NULL;
	MenuItem->LeftEdge = LeftEdge;
	MenuItem->TopEdge = TopEdge;
	MenuItem->Width = Width;
	MenuItem->Height = Height;

	MenuItem->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;

	if (Key) {
		MenuItem->Flags = MenuItem->Flags | COMMSEQ;
	}
	if (Exclude) {
		MenuItem->Flags = MenuItem->Flags | CHECKIT;
	}
/*
   if (Key==NULL)   
   {
   MenuItem->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
   }
   else
   {
   MenuItem->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP | COMMSEQ;
   }
 */

	MenuItem->MutualExclude = Exclude;
	MenuItem->ItemFill = (APTR) IntuiText;
	MenuItem->SelectFill = NULL;
	MenuItem->Command = (BYTE) Key;
	MenuItem->SubItem = NULL;
	MenuItem->NextSelect = MENUNULL;

	return MenuItem;
}



/*
 * ========
 * MakeMenu
 * ========
 */

/*
 * Revision     : v0.1.8
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : NOT KNOWN (Before 2nd September 1995)
 *
 * Notes: A simple routine to help GUI development.
 */

struct Menu *MakeMenu(int LeftEdge, int TopEdge, int Width, char *Title, struct MenuItem *MenuItem)
{
	struct Menu *Menu;

	Menu = (struct Menu *) malloc(sizeof(struct Menu));
	if (!Menu) {
	}
	Menu->NextMenu = NULL;
	Menu->LeftEdge = LeftEdge;
	Menu->TopEdge = TopEdge;
	Menu->Width = Width;
	Menu->Height = 0;
	Menu->Flags = MENUENABLED;
	Menu->MenuName = Title;
	Menu->FirstItem = MenuItem;

	return Menu;
}



/*
 * ==========
 * MakeGadget
 * ==========
 */

/*
 * Revision     : v0.2.1a
 * Introduced   : 2nd September 1995
 * Last updated : 9th October 1995
 *
 * Notes: Not complete yet. The Type argument is currently not supported so
 * routine is only capable of producing boring click-me-to-continue type
 * gadgets.
 */

struct Gadget *MakeGadget(int LeftEdge, int TopEdge, int Width, int Height, int Offset, struct Window *Window, char *Title, int Type)
{
	int Result;
	int TextX;

	struct IntuiText *NewTextAddress = NULL;
	struct Gadget *NewGadgetAddress = NULL;

	NewGadgetAddress = (struct Gadget *) malloc(sizeof(struct Gadget));
	if (!NewGadgetAddress) {
	}
	NewGadgetAddress->NextGadget = NULL;
/*
   NewGadgetAddress->LeftEdge = LeftEdge;
   NewGadgetAddress->TopEdge = TopEdge;
 */
	NewGadgetAddress->Width = Width;
	NewGadgetAddress->Height = Height;

	NewGadgetAddress->Flags = GADGIMAGE | GADGHIMAGE;
	NewGadgetAddress->Activation = RELVERIFY;
	NewGadgetAddress->GadgetType = BOOLGADGET;
	if (Type == gadget_Button) {
		NewGadgetAddress->GadgetRender = &image_Button64;
		NewGadgetAddress->SelectRender = &image_Button64Selected;
	}
	else if (Type == gadget_Mutual) {
		NewGadgetAddress->Activation = TOGGLESELECT | RELVERIFY;
		NewGadgetAddress->GadgetRender = &image_MutualGadget;
		NewGadgetAddress->SelectRender = &image_MutualGadgetSelected;
	}
	else {
		NewGadgetAddress->Flags = GADGHCOMP;
	}

	NewGadgetAddress->GadgetText = NULL;
	NewGadgetAddress->MutualExclude = NULL;
	NewGadgetAddress->SpecialInfo = NULL;
	NewGadgetAddress->GadgetID = 0;
	NewGadgetAddress->UserData = NULL;

	LeftEdge = (LeftEdge * gui_GridWidth) + gui_WindowOffsetLeftEdge;
	TopEdge = (TopEdge * gui_GridHeight) + gui_WindowOffsetTopEdge + 1 + ((gui_GridHeight - 7) * 2);

	NewGadgetAddress->LeftEdge = LeftEdge;
	NewGadgetAddress->TopEdge = TopEdge;
	NewGadgetAddress->Width = Width;
	NewGadgetAddress->Height = Height;

	if (Type == gadget_Button) {
		TextX = (Width / 2) - (strlen(Title) * 4);
	}
	else if (Type == gadget_Mutual) {
		TextX = 24;
	}
	NewTextAddress = (struct IntuiText *) malloc(sizeof(struct IntuiText));
	if (!NewTextAddress) {
	}
	NewTextAddress->FrontPen = 1;
	NewTextAddress->BackPen = 3;
	NewTextAddress->DrawMode = JAM1;
	NewTextAddress->LeftEdge = TextX;
	NewTextAddress->TopEdge = 3;
	NewTextAddress->ITextFont = NULL;
	NewTextAddress->IText = (UBYTE *) Title;
	NewTextAddress->NextText = NULL;

	NewGadgetAddress->GadgetText = NewTextAddress;

	if (NewGadgetAddress->Activation & TOGGLESELECT) {
		if (Title) {
			if (Offset) {
				NewTextAddress->LeftEdge = Offset;
			}
		}
	}
	if (Window) {
		Result = AddGadget(Window, NewGadgetAddress, -1);
		RefreshGadgets(NewGadgetAddress, Window, NULL);
	}
	return NewGadgetAddress;
}



/*
 * ========
 * ShowText
 * ========
 */

/*
 * Revision     : v0.2.0
 * Introduced   : 2nd September 1995
 * Last updated : 9th September 1995
 *
 * Notes: A simple routine to help any future GUI development.
 */

void ShowText(int LeftEdge, int TopEdge, struct Window *Window, char *TextString)
{
	if (ScreenType == CUSTOMSCREEN) {
		SetAPen(Window->RPort, 0);
		SetBPen(Window->RPort, 6);
	}
	else {
		SetAPen(Window->RPort, 1);
		SetBPen(Window->RPort, 0);
	}

	LeftEdge = (LeftEdge * gui_GridWidth);
	TopEdge = (TopEdge * gui_GridHeight) + ((gui_GridHeight - 7) * 2);

	if (Window->Flags & GIMMEZEROZERO) {
		TopEdge = TopEdge + 8;
	}
	else {
		LeftEdge = LeftEdge + gui_WindowOffsetLeftEdge;
		TopEdge = TopEdge + gui_WindowOffsetTopEdge + 8;
	}

	Move(Window->RPort, LeftEdge, TopEdge);
	Text(Window->RPort, TextString, strlen(TextString));
}



/*
 * ====
 * Rule
 * ====
 */

/*
 * Revision     : v0.2.0
 * Introduced   : 2nd September 1995
 * Last updated : 9th September 1995
 *
 * Notes: A simple routine to help any future GUI development.
 */

void Rule(int LeftEdge, int TopEdge, int Width, struct Window *Window)
{
	int RightEdge;

	LeftEdge = (LeftEdge * gui_GridWidth) + gui_WindowOffsetLeftEdge;
	TopEdge = (TopEdge * gui_GridHeight) + 12 + gui_WindowOffsetTopEdge;	/* + 44 */

	RightEdge = Width + LeftEdge - 1;

	if (ScreenType == CUSTOMSCREEN) {
		SetAPen(Window->RPort, 0);
	}
	else {
		SetAPen(Window->RPort, 1);
	}

	Move(Window->RPort, LeftEdge, TopEdge);
	Draw(Window->RPort, RightEdge, TopEdge);

	if (ScreenType == CUSTOMSCREEN) {
		SetAPen(Window->RPort, 15);
	}
	else {
		SetAPen(Window->RPort, 2);
	}

	Move(Window->RPort, LeftEdge, TopEdge + 1);
	Draw(Window->RPort, RightEdge, TopEdge + 1);
}



/*
 * ==========
 * InsertDisk
 * ==========
 */

/*
 * Revision     : v0.2.1
 * Introduced   : 24th September 1995
 * Last updated : 24th September 1995
 *
 * Notes: A simple routine to help any future GUI development.
 */

int InsertDisk(int Drive)
{
	struct FileRequester *FileRequester = NULL;
	char Filename[256];
	int Success = FALSE;

	if (FileRequester = (struct FileRequester *) AllocAslRequestTags(ASL_FileRequest, TAG_DONE)) {
		if (AslRequestTags(FileRequester,
						   ASLFR_Screen, ScreenMain,
						   ASLFR_TitleText, "Select file",
						   ASLFR_PositiveText, "Ok",
						   ASLFR_NegativeText, "Cancel",
						   TAG_DONE)) {
			printf("File selected: %s/%s\n", FileRequester->rf_Dir, FileRequester->rf_File);
			printf("Number of files : %d\n", FileRequester->fr_NumArgs);

			SIO_Dismount(Drive);

			if (FileRequester->rf_Dir) {
				sprintf(Filename, "%s/%s", FileRequester->rf_Dir, FileRequester->rf_File);
			}
			else {
				sprintf(Filename, "%s", FileRequester->rf_File);
			}

			if (!SIO_Mount(Drive, Filename)) {
			}
			else {
				Success = TRUE;
			}
		}
		else {
			/*
			 * Cancelled
			 */
		}
	}
	else {
		printf("Unable to create requester\n");
	}

	return Success;
}



/*
 * =========
 * InsertROM
 * =========
 */

/*
 * Revision     : v0.3.3
 * Introduced   : 10th March 1996
 * Last updated : 10th March 1996
 */

int InsertROM(int CartType)
{
	struct FileRequester *FileRequester = NULL;
	char Filename[256];
	int Success = FALSE;

	if (FileRequester = (struct FileRequester *) AllocAslRequestTags(ASL_FileRequest, TAG_DONE)) {
		if (AslRequestTags(FileRequester,
						   ASLFR_Screen, ScreenMain,
						   ASLFR_TitleText, "Select file",
						   ASLFR_PositiveText, "Ok",
						   ASLFR_NegativeText, "Cancel",
						   TAG_DONE)) {
			printf("File selected: %s/%s\n", FileRequester->rf_Dir, FileRequester->rf_File);
			printf("Number of files : %d\n", FileRequester->fr_NumArgs);

			if (FileRequester->rf_Dir) {
				sprintf(Filename, "%s/%s", FileRequester->rf_Dir, FileRequester->rf_File);
			}
			else {
				sprintf(Filename, "%s", FileRequester->rf_File);
			}

			if (CartType == 0) {
				Remove_ROM();
				if (Insert_8K_ROM(Filename)) {
					Coldstart();
				}
			}
			else if (CartType == 1) {
				Remove_ROM();
				if (Insert_16K_ROM(Filename)) {
					Coldstart();
				}
			}
			else if (CartType == 2) {
				Remove_ROM();
				if (Insert_OSS_ROM(Filename)) {
					Coldstart();
				}
			}
		}
		else {
			/*
			 * Cancelled
			 */
		}
	}
	else {
		printf("Unable to create requester\n");
	}

	return Success;
}



insert_rom_callback()
{
/*
   xv_set (chooser,
   FRAME_LABEL, "ROM Selector",
   FILE_CHOOSER_DIRECTORY, ATARI_ROM_DIR,
   FILE_CHOOSER_NOTIFY_FUNC, rom_change,
   XV_SHOW, TRUE,
   NULL);
 */
}



/*
 * ============
 * SetupDisplay
 * ============
 */

/*
 * Revision     : v0.3.3
 * Introduced   : 10th March 1996
 * Last updated : 10th March 1996
 */

SetupDisplay()
{
	struct NewWindow NewWindow;
	int i;

	/*
	 * ===========
	 * Screen Pens
	 * ===========
	 */

	WORD ScreenPens[13] =
	{
		15,						/* Unknown */
		15,						/* Unknown */
		0,						/* Windows titlebar text when inactive */
		15,						/* Windows bright edges */
		0,						/* Windows dark edges */
		120,					/* Windows titlebar when active */
		0,						/* Windows titlebar text when active */
		4,						/* Windows titlebar when inactive */
		15,						/* Unknown */
		0,						/* Menubar text */
		15,						/* Menubar */
		0,						/* Menubar base */
		-1
	};

	/*
	 * =============
	 * Create Screen
	 * =============
	 */

	if (CustomScreen) {
		ScreenType = CUSTOMSCREEN;

		ScreenWidth = ATARI_WIDTH - 64;		/* ATARI_WIDTH + 8; */
		ScreenHeight = ATARI_HEIGHT;	/* ATARI_HEIGHT + 13; */

		if (ChipSet == AGA_ChipSet) {
			ScreenDepth = 8;
		}
		else {
			ScreenDepth = 5;
		}

		NewScreen.LeftEdge = 0;
		NewScreen.TopEdge = 0;
		NewScreen.Width = ScreenWidth;
		NewScreen.Height = ScreenHeight;
		NewScreen.Depth = ScreenDepth;
		NewScreen.DetailPen = 1;
		NewScreen.BlockPen = 2;	/* 2 */
		NewScreen.ViewModes = NULL;
		NewScreen.Type = CUSTOMSCREEN;
		NewScreen.Font = NULL;
		NewScreen.DefaultTitle = ATARI_TITLE;
		NewScreen.Gadgets = NULL;
		NewScreen.CustomBitMap = NULL;

		ScreenMain = (struct Screen *) OpenScreenTags
			(
				&NewScreen,
				SA_Left, 0,
				SA_Top, 0,
				SA_Width, ScreenWidth,
				SA_Height, ScreenHeight,
				SA_Depth, ScreenDepth,
				SA_DetailPen, 1,
				SA_BlockPen, 2,	/* 2 */
				SA_Pens, ScreenPens,
				SA_Title, ATARI_TITLE,
				SA_Type, CUSTOMSCREEN,
/*
   SA_Overscan, OSCAN_STANDARD,
 */
/*
   SA_DisplayID, ScreenID,
 */
				SA_AutoScroll, TRUE,
/*
   SA_Interleaved, TRUE,
 */
				TAG_DONE
			);

		if (ChipSet == AGA_ChipSet) {
			TotalColours = 256;
		}
		else {
			TotalColours = 16;
		}

		for (i = 0; i < TotalColours; i++) {
			int rgb = colortable[i];
			int red;
			int green;
			int blue;

			red = (rgb & 0x00ff0000) >> 20;
			green = (rgb & 0x0000ff00) >> 12;
			blue = (rgb & 0x000000ff) >> 4;

			SetRGB4(&ScreenMain->ViewPort, i, red, green, blue);
		}

		image_Button64.PlanePick = 9;
		image_Button64.PlaneOnOff = (UBYTE) 6;
		image_Button64Selected.PlanePick = 9;
		image_Button64Selected.PlaneOnOff = (UBYTE) 6;
		image_MutualGadget.PlanePick = 9;
		image_MutualGadget.PlaneOnOff = (UBYTE) 6;
		image_MutualGadgetSelected.PlanePick = 9;
		image_MutualGadgetSelected.PlaneOnOff = (UBYTE) 6;
	}
	else {
		ScreenType = WBENCHSCREEN;

		ScreenWidth = ATARI_WIDTH;	/* ATARI_WIDTH + 8; */
		ScreenHeight = ATARI_HEIGHT;	/* ATARI_HEIGHT + 13; */
	}

	/*
	 * =============
	 * Create Window
	 * =============
	 */

	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 148;

	if (CustomScreen) {
		NewWindow.LeftEdge = 0;
		NewWindow.TopEdge = 0;
		NewWindow.Width = ScreenWidth;	/* ATARI_WIDTH + 8; */
		NewWindow.Height = ScreenHeight;	/* ATARI_HEIGHT + 13; */
		NewWindow.IDCMPFlags = SELECTDOWN | SELECTUP | MOUSEBUTTONS | MOUSEMOVE | MENUPICK | MENUVERIFY | MOUSEBUTTONS | GADGETUP | RAWKEY | VANILLAKEY;

		/*
		 * If you use the ClickToFront commodity it might be a good idea to
		 * enable the BACKDROP option in the NewWindow.Flags line below.
		 */

		NewWindow.Flags = /* BACKDROP | */ REPORTMOUSE | BORDERLESS | GIMMEZEROZERO | SMART_REFRESH | ACTIVATE;
		NewWindow.Title = NULL;
	}
	else {
		NewWindow.LeftEdge = 0;
		NewWindow.TopEdge = 11;
		NewWindow.Width = ScreenWidth + 8;	/* ATARI_WIDTH + 8; */
		NewWindow.Height = ScreenHeight + 13;	/* ATARI_HEIGHT + 13; */
		NewWindow.IDCMPFlags = SELECTDOWN | SELECTUP | MOUSEBUTTONS | MOUSEMOVE | CLOSEWINDOW | MENUPICK | MENUVERIFY | MOUSEBUTTONS | GADGETUP | RAWKEY | VANILLAKEY;
		NewWindow.Flags = REPORTMOUSE | WINDOWCLOSE | GIMMEZEROZERO | WINDOWDRAG | WINDOWDEPTH | SMART_REFRESH | ACTIVATE;
		NewWindow.Title = ATARI_TITLE;
	}

	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Screen = ScreenMain;
	NewWindow.Type = ScreenType;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 50;
	NewWindow.MinHeight = 11;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowMain = (struct Window *) OpenWindowTags
		(
			&NewWindow,
			WA_NewLookMenus, TRUE,
			WA_MenuHelp, TRUE,
			WA_ScreenTitle, ATARI_TITLE,
/*
   WA_Zoom, Zoomdata,
 */
			TAG_DONE
		);

	if (!WindowMain) {
		printf("Failed to create window\n");
		Atari_Exit(0);
	}
}



/*
 * =======
 * Iconify
 * =======
 */

/*
 * Revision     : v0.3.3
 * Introduced   : 10th March 1996
 * Last updated : 10th March 1996
 */

Iconify()
{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow NewWindow;

	int QuitRoutine;

	ClearMenuStrip(WindowMain);

	if (WindowMain) {
		CloseWindow(WindowMain);
	}
	if (ScreenMain) {
		CloseScreen(ScreenMain);
	}
	NewWindow.LeftEdge = 0;
	NewWindow.TopEdge = 11;
	NewWindow.Width = 112;
	NewWindow.Height = 21;
	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 15;
	NewWindow.IDCMPFlags = CLOSEWINDOW;
	NewWindow.Flags = WINDOWCLOSE | WINDOWDRAG | ACTIVATE;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Title = "Atari800e";
	NewWindow.Screen = NULL;
	NewWindow.Type = WBENCHSCREEN;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 92;
	NewWindow.MinHeight = 21;
	NewWindow.MaxWidth = 1280;
	NewWindow.MaxHeight = 512;

	WindowIconified = (struct Window *) OpenWindowTags
		(
			&NewWindow,
			WA_NewLookMenus, TRUE,
			WA_MenuHelp, TRUE,
			WA_ScreenTitle, ATARI_TITLE,
			TAG_DONE
		);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE) {
		while (IntuiMessage = (struct IntuiMessage *) GetMsg(WindowIconified->UserPort)) {
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg(IntuiMessage);

			switch (Class) {
			case CLOSEWINDOW:
				QuitRoutine = TRUE;
				break;
			default:
				break;
			}
		}
	}

	CloseWindow(WindowIconified);

	SetupDisplay();

	SetMenuStrip(WindowMain, menu_Project);
}
