/*
 * ==========================
 * Atari800 - Amiga Port Code
 * ==========================
 */

/*
 * Revision     : v0.2.1
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 12th October 1995
 *
 *
 *
 * Introduction:
 *
 * This file contains all the additional code required to get the Amiga
 * port of the Atari800 emulator up and running.
 *
 *
 *
 * Notes:
 *
 *
 *
 * v0.1.9 (I think!)
 *
 * o Now supports AGA graphics via -aga startup option. This option provides
 *   support for the Atari's full 256 colour palette.
 *     The emulator will default to this mode.
 *
 * o Should support ECS graphics via -ecs startup option. Unfortunately, I
 *   have not been able to test this on a real ECS based Amiga so I would
 *   appreciate it if someone would let me know whether it works or not.
 *     The only difference between this mode and the AGA mode is that
 *   the screen display is rendered with 32 pens as opposed to using the
 *   Atari's full 256 colour palette.
 *
 * o Should support OCS graphics via -ocs startup option. Unfortunately, I
 *   have not been able to test this on a real OCS based Amiga so I would
 *   appreciate it if someone would let me know whether it works or not. Of
 *   the three modes, this is the least likely to work. There is no real
 *   difference between this and the ECS mode, so I'm just hoping it does.
 *   However, certain operating system calls may not work, but without
 *   further details, I have no idea which these may be.
 *     Anyway, in this mode, the screen display should be rendered with 32
 *   pens just like the ECS mode.
 *
 * o The emulator has been tested on an Amiga A1200 using the OCS, ECS and
 *   AGA chipset options on powerup. However, due to the fact that the A1200
 *   uses Workbench 3.0, I have no idea whether it will work on real OCS and
 *   ECS based Amiga's. Sorry! I would appreciate it if someone would let me
 *   know whether it does work on these machines. Thanks!
 *
 *
 *
 * v0.2.0 (I think!)
 *
 * o Hooks have been provided for sound support as required by v0.1.9.
 *   However, sound is not currently supported (And probably won't be for a
 *   while yet!). If anyone is interested in developing this area please
 *   feel free.
 *
 * o Upon compilation you will reveive one warning regarding the definition
 *   of CLOSE. A standard Amiga define is conflicting with one defined
 *   within the "atari.h" file. This is unlikely to be corrected. Apart from
 *   this one warning, there should now be no other warnings of any kind.
 *
 * o Thanks to a little experimentation you can now see the menus more
 *   clearly and the windows (all two of them!) have been given a much
 *   needed facelift.
 *
 * o -grey startup option added. In this mode you will get a grey scale
 *   screen display.
 *
 * o -colour startup option added. In AGA modes you will get the full 256
 *   colour palette of the Atari. In OCS and ECS modes you will get the
 *   best representation the program can get using 32 pens.
 *     The emulator will default to this mode.
 *
 * o -wb startup option added. This option tells the emulator to make an
 *   attempt at opening a window on the Workbench screen in which it should
 *   render the display. At the moment this is at a prelimary stage of
 *   development. Setting this will automatically enable the ECS_ChipSet
 *   version. As yet, there is no specific AGA version that takes into
 *   account the AGA's increased colour palette. However, bearing in mind
 *   the amount of colour you can get on screen using the ECS engine, is
 *   there really any point?
 *     In this mode you may also quit the emulator by clicking the main
 *   windows close gadget.
 *     The advantage that this mode has over all over modes is that you get
 *   to see the whole screen without any of the edges cut off. The big
 *   disadvantage is that if the program currently being run on the emulator
 *   uses a lot of colours it may overwrite the last 4 colours that
 *   Workbench has defined. Without knowing how to find out the depth of the
 *   Workbench screen, I'm a bit stuck on this one.
 *     For the best results, use this mode on nothing less than a 16 colour
 *   screen.
 *
 * o -paddle startup option added. This option enables the mouse based
 *   paddle controller emulation.
 *
 * o -joystick startup option added. Enables the joystick controller
 *   emulation.
 *     The emulator will default to this mode.
 *
 *
 *
 * v0.2.1
 *
 * o The Project menu has been renamed the Amiga menu.
 *
 * o A new menu named Disk has been added. This allows you to select a boot
 *   disk and allows you to insert a disk into a virtual drive or remove a
 *   disk from a virtual drive without having to reboot.
 *
 * o The Help key is now active via the Console/Help menu-item. Please note:
 *   this menu-item will only be enabled if you run the emulator with
 *   either the -xl or -xe startup option.
 *
 * o Added images to gadgets.
 *
 * o The AGA Setup window has been updated.
 *
 * o The Prefs/Controller menu has been updated ready for future
 *   improvements regarding controllers.
 *
 *
 *
 * Future improvements:
 *
 * o Further GUI improvement with regards to the controller menu-item
 *
 * o Possible introduction of AmigaGuide documentation
 *
 *
 *
 * BELATED THANK YOU:
 *
 * Thanks must go to (some guy on a computer) going by the name (I presume!)
 * of D. Dussia who told me about one stupid mistake I made regarding
 * libraries. ie open v39 when pre-AGA machines don't have them (der,
 * stooopid!). Also told me that the emulator would compile using SAS/C if
 * you tell it to compile in FAR mode and that you define Dice_c.
 * Unfortunately, not owning SAS/C, I'm not too sure what this means, but if
 * you do then you're free to have a go.
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

#include "atari.h"
#include "colours.h"
#include "system.h"



#define GAMEPORT0 0
#define GAMEPORT1 1

#define OCS_ChipSet 0
#define ECS_ChipSet 1
#define AGA_ChipSet 2

#ifdef DICE_C
static struct IntuitionBase	*IntuitionBase = NULL;
static struct GfxBase		*GfxBase = NULL;
static struct LayersBase	*LayersBase = NULL;
static struct Library *AslBase = NULL;
#endif

#ifdef GNU_C
struct IntuitionBase	*IntuitionBase = NULL;
struct GfxBase		*GfxBase = NULL;
struct LayersBase	*LayersBase = NULL;
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

int old_stick_0 = 0x1f;
int old_stick_1 = 0x1f;

static int consol;
static int trig0;
static int stick0;

struct InputEvent gameport_data;
struct MsgPort *gameport_msg_port;
struct IOStdReq *gameport_io_msg;
BOOL gameport_error;

static UBYTE	*image_data;

static UWORD ScreenType;
static struct Screen *ScreenMain = NULL;
static struct NewScreen NewScreen;
static struct Image image_Screen;
static int ScreenID;
static int ScreenWidth;
static int ScreenHeight;
static int ScreenDepth;
static int TotalColours;

static struct Menu *menu_Project = NULL;
static struct MenuItem *menu_Project00 = NULL;
static struct MenuItem *menu_Project01 = NULL;
static struct MenuItem *menu_Project02 = NULL;
static struct MenuItem *menu_Project03 = NULL;

static struct Menu *menu_Disk = NULL;
static struct MenuItem *menu_Disk00 = NULL;
static struct MenuItem *menu_Disk01 = NULL;
static struct MenuItem *menu_Disk01s00 = NULL;
static struct MenuItem *menu_Disk01s01 = NULL;
static struct MenuItem *menu_Disk01s02 = NULL;
static struct MenuItem *menu_Disk01s03 = NULL;
static struct MenuItem *menu_Disk01s04 = NULL;
static struct MenuItem *menu_Disk01s05 = NULL;
static struct MenuItem *menu_Disk01s06 = NULL;
static struct MenuItem *menu_Disk01s07 = NULL;
static struct MenuItem *menu_Disk02 = NULL;
static struct MenuItem *menu_Disk02s00 = NULL;
static struct MenuItem *menu_Disk02s01 = NULL;
static struct MenuItem *menu_Disk02s02 = NULL;
static struct MenuItem *menu_Disk02s03 = NULL;
static struct MenuItem *menu_Disk02s04 = NULL;
static struct MenuItem *menu_Disk02s05 = NULL;
static struct MenuItem *menu_Disk02s06 = NULL;
static struct MenuItem *menu_Disk02s07 = NULL;

static struct Menu *menu_Console = NULL;
static struct MenuItem *menu_Console00 = NULL;
static struct MenuItem *menu_Console01 = NULL;
static struct MenuItem *menu_Console02 = NULL;
static struct MenuItem *menu_Console03 = NULL;
static struct MenuItem *menu_Console04 = NULL;
static struct MenuItem *menu_Console05 = NULL;

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

static void DisplayAboutWindow ();
static void DisplayNotSupportedWindow ();
static void DisplaySetupWindow ();
static void DisplayControllerWindow ();
static int DisplayYesNoWindow ();
static int InsertDisk ();
static struct Gadget *MakeGadget ();
static struct MenuItem *MakeMenuItem ();
static struct Menu *MakeMenu ();
static void Rule ();
static void ShowText ();

static int gui_GridWidth = 4;
static int gui_GridHeight = 7;
static int gui_WindowOffsetLeftEdge = 0; /* 4 */
static int gui_WindowOffsetTopEdge = 0; /* 11 */

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

UWORD data_Button64[]=
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

UWORD data_Button64Selected[]=
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

UWORD data_MutualGadget[]=
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

UWORD data_MutualGadgetSelected[]=
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

void Atari_Initialise (int *argc, char *argv[])
{
	struct NewWindow	NewWindow;

	char *ptr;
	int i;
	int j;
	struct IntuiMessage *IntuiMessage;

	ULONG Class;
	USHORT Code;
	APTR Address;

	int QuitRoutine;

	/*
	 * ===========
	 * Screen Pens
	 * ===========
	 */

	WORD ScreenPens[13] =
	{
		15, /* Unknown */
		15, /* Unknown */
		0, /* Windows titlebar text when inactive */
		15, /* Windows bright edges */
		0, /* Windows dark edges */
		120, /* Windows titlebar when active */
		0, /* Windows titlebar text when active */
		4, /* Windows titlebar when inactive */
		15, /* Unknown */
		0, /* Menubar text */
		15, /* Menubar */
		0, /* Menubar base */
		-1
	};

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
	 *
	 *
	 *
	 */

	chip_Button64 = (UWORD *) AllocMem (sizeof (data_Button64), MEMF_CHIP);
	chip_Button64Selected = (UWORD *) AllocMem (sizeof (data_Button64Selected), MEMF_CHIP);
	chip_MutualGadget = (UWORD *) AllocMem (sizeof (data_MutualGadget), MEMF_CHIP);
	chip_MutualGadgetSelected = (UWORD *) AllocMem (sizeof (data_MutualGadgetSelected), MEMF_CHIP);

	memcpy (chip_Button64, data_Button64, sizeof (data_Button64));
	memcpy (chip_Button64Selected, data_Button64Selected, sizeof (data_Button64Selected));
	memcpy (chip_MutualGadget, data_MutualGadget, sizeof (data_MutualGadget));
	memcpy (chip_MutualGadgetSelected, data_MutualGadgetSelected, sizeof (data_MutualGadgetSelected));

	/*
	 *
	 *
	 *
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

	for (i=j=1;i<*argc;i++)
	{
/*
		printf("%d: %s\n", i,argv[i]);
*/

		if (strcmp(argv[i], "-ocs") == 0)
		{
			printf ("Requested OCS graphics mode... Accepted!\n");
			ChipSet = OCS_ChipSet;
			LibraryVersion = 37;
		}
		else if (strcmp(argv[i], "-ecs") == 0)
		{
			printf ("Requested ECS graphics mode... Accepted!\n");
			ChipSet = ECS_ChipSet;
			LibraryVersion = 37;
		}
		else if (strcmp(argv[i], "-aga") == 0)
		{
			printf ("Requested AGA graphics mode... Accepted!\n");
			printf ("  Enabled custom screen automatically\n");
			ChipSet = AGA_ChipSet;
			CustomScreen = TRUE;
			LibraryVersion = 39;
		}
		else if (strcmp(argv[i], "-grey") == 0)
		{
			printf ("Requested grey scale display... Accepted!\n");
			ColourEnabled = FALSE;
		}
		else if (strcmp(argv[i], "-colour") == 0)
		{
			printf ("Requested colour display... Accepted!\n");
			ColourEnabled = TRUE;
		}
		else if (strcmp(argv[i], "-wb") == 0)
		{
			printf ("Requested Workbench window... Accepted!\n");
			CustomScreen = FALSE;
			LibraryVersion = 37;
			if (ChipSet == AGA_ChipSet)
			{
				printf ("  ECS chip set automatically enabled\n");
				ChipSet = ECS_ChipSet;
			}
		}
		else if (strcmp(argv[i], "-joystick") == 0)
		{
			printf ("Requested joystick controller... Accepted!\n");
			Controller = controller_Joystick;
		}
		else if (strcmp(argv[i], "-paddle") == 0)
		{
			printf ("Requested paddle controller... Accepted!\n");
			Controller = controller_Paddle;
		}
		else
		{
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	IntuitionBase = (struct IntuitionBase*) OpenLibrary ("intuition.library", LibraryVersion);
	if (!IntuitionBase)
	{
		printf ("Failed to open intuition.library\n");
		Atari_Exit (0);
	}

	GfxBase = (struct GfxBase*) OpenLibrary ("graphics.library", LibraryVersion);
	if (!GfxBase)
	{
		printf ("Failed to open graphics.library\n");
		Atari_Exit (0);
	}

	LayersBase = (struct LayersBase*) OpenLibrary ("layers.library", LibraryVersion);
	if (!LayersBase)
	{
		printf ("Failed to open layers.library\n");
		Atari_Exit (0);
	}

	AslBase = (struct Library*) OpenLibrary ("asl.library", LibraryVersion);
	if (!AslBase)
	{
		printf ("Failed to open asl.library\n");
		Atari_Exit (0);
	}

	data_Screen = (UWORD *) AllocMem (46080 * 2, MEMF_CHIP);
	if (!data_Screen)
	{
		printf ("Oh BUGGER!\n");
	}

	printf ("data_Screen = %x\n", data_Screen);

	/*
	 * ==============
	 * Setup Joystick
	 * ==============
	 */

	gameport_msg_port = (struct MsgPort *) CreatePort (0, 0);
	if (!gameport_msg_port)
	{
		printf ("Failed to create gameport_msg_port\n");
		Atari_Exit (0);
	}

	gameport_io_msg = (struct IOStdReq *) CreateStdIO (gameport_msg_port);
	if (!gameport_io_msg)
	{
		printf ("Failed to create gameport_io_msg\n");
		Atari_Exit (0);
	}

	gameport_error = OpenDevice ("gameport.device", GAMEPORT1, gameport_io_msg, 0xFFFF);
	if (gameport_error)
	{
		printf ("Failed to open the gameport.device\n");
		Atari_Exit (0);
	}

	{
		BYTE type = 0;

		gameport_io_msg->io_Command = GPD_ASKCTYPE;
		gameport_io_msg->io_Length = 1;
		gameport_io_msg->io_Data = (APTR) &type;

		DoIO (gameport_io_msg);

		if (type)
		{
			printf ("gameport already in use\n");
			gameport_error = TRUE;
		}
	}

	{
		BYTE type = GPCT_ABSJOYSTICK;

		gameport_io_msg->io_Command = GPD_SETCTYPE;
		gameport_io_msg->io_Length = 1;
		gameport_io_msg->io_Data = (APTR) &type;

		DoIO (gameport_io_msg);

		if (gameport_io_msg->io_Error)
		{
			printf ("Failed to set controller type\n");
		}

	}

	{
		struct GamePortTrigger gpt;

		gpt.gpt_Keys = GPTF_DOWNKEYS | GPTF_UPKEYS;
		gpt.gpt_Timeout = 0;
		gpt.gpt_XDelta = 1;
		gpt.gpt_YDelta = 1;

		gameport_io_msg->io_Command = GPD_SETTRIGGER;
		gameport_io_msg->io_Length = sizeof (gpt);
		gameport_io_msg->io_Data = (APTR) &gpt;

		DoIO (gameport_io_msg);

		if (gameport_io_msg->io_Error)
		{
			printf ("Failed to set controller trigger\n");
		}
	}

	{
		struct InputEvent *Data;

		gameport_io_msg->io_Command = GPD_READEVENT;
		gameport_io_msg->io_Length = sizeof (struct InputEvent);
		gameport_io_msg->io_Data = (APTR) &gameport_data;
		gameport_io_msg->io_Flags = 0;
	}

	SendIO (gameport_io_msg);

	ScreenType = WBENCHSCREEN;

	if (ChipSet == AGA_ChipSet)
	{
		DisplaySetupWindow ();
	}

	/*
	 * =============
	 * Create Screen
	 * =============
	 */

	if (CustomScreen)
	{
		ScreenType = CUSTOMSCREEN;

		ScreenWidth = ATARI_WIDTH - 64; /* ATARI_WIDTH + 8; */
		ScreenHeight = ATARI_HEIGHT; /* ATARI_HEIGHT + 13; */

		if (ChipSet == AGA_ChipSet)
		{
			ScreenDepth = 8;
		}
		else
		{
			ScreenDepth = 5;
		}

		NewScreen.LeftEdge = 0;
		NewScreen.TopEdge = 0;
		NewScreen.Width = ScreenWidth;
		NewScreen.Height = ScreenHeight;
		NewScreen.Depth = ScreenDepth;
		NewScreen.DetailPen = 1;
		NewScreen.BlockPen = 2; /* 2 */
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
			SA_BlockPen, 2, /* 2 */
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

		if (ChipSet == AGA_ChipSet)
		{
			TotalColours = 256;
		}
		else
		{
			TotalColours = 16;
		}

		for (i=0;i<TotalColours;i++)
		{
			int rgb = colortable[i];
			int red;
			int green;
			int blue;

			red = (rgb & 0x00ff0000) >> 20;
			green = (rgb & 0x0000ff00) >> 12;
			blue = (rgb & 0x000000ff) >> 4;

			SetRGB4 (&ScreenMain->ViewPort, i, red, green, blue);
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
	else
	{
		ScreenType = WBENCHSCREEN;

		ScreenWidth = ATARI_WIDTH; /* ATARI_WIDTH + 8; */
		ScreenHeight = ATARI_HEIGHT; /* ATARI_HEIGHT + 13; */
	}

	/*
	 * =============
	 * Create Window
	 * =============
	 */

	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 148;

	if (CustomScreen)
	{
		NewWindow.LeftEdge = 0;
		NewWindow.TopEdge = 0;
		NewWindow.Width = ScreenWidth; /* ATARI_WIDTH + 8; */
		NewWindow.Height = ScreenHeight; /* ATARI_HEIGHT + 13; */
		NewWindow.IDCMPFlags = SELECTDOWN | SELECTUP | MOUSEBUTTONS | MOUSEMOVE | MENUPICK | MENUVERIFY | MOUSEBUTTONS | GADGETUP | RAWKEY | VANILLAKEY;

		/*
		 * If you use the ClickToFront commodity it might be a good idea to
		 * enable the BACKDROP option in the NewWindow.Flags line below.
		 */

		NewWindow.Flags = /* BACKDROP | */ REPORTMOUSE | BORDERLESS | GIMMEZEROZERO | SMART_REFRESH | ACTIVATE;
		NewWindow.Title = NULL;
	}
	else
	{
		NewWindow.LeftEdge = 0;
		NewWindow.TopEdge = 11;
		NewWindow.Width = ScreenWidth + 8; /* ATARI_WIDTH + 8; */
		NewWindow.Height = ScreenHeight + 13; /* ATARI_HEIGHT + 13; */
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

	WindowMain = (struct Window*) OpenWindowTags
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

	if (!WindowMain)
	{
		printf ("Failed to create window\n");
		Atari_Exit (0);
	}

	DisplayAboutWindow ();

/*
 * Setup Project Menu
 */

	menu_Project00 = MakeMenuItem (0, 0, 96, 10, "Iconify", NULL, NULL);
	menu_Project01 = MakeMenuItem	(0, 15, 96, 10, "Help", 'H', NULL);
	menu_Project02 = MakeMenuItem (0, 30, 96, 10, "About", '?', NULL);
	menu_Project03 = MakeMenuItem	(0, 45, 96, 10, "Quit", 'N', NULL);

	menu_Project00->NextItem = menu_Project01;
	menu_Project01->NextItem = menu_Project02;
	menu_Project02->NextItem = menu_Project03;

	menu_Project = MakeMenu (0, 0, 48, "Amiga", menu_Project00);

/*
 * Setup Disk Menu
 */

	menu_Disk00 = MakeMenuItem (0, 0, 88, 10, "Boot disk", NULL, NULL);
	menu_Disk01 = MakeMenuItem (0, 10, 88, 10, "Insert disk", NULL, NULL);
		menu_Disk01s00 = MakeMenuItem (72, 0, 80, 10, "Drive 1...", NULL, NULL);
		menu_Disk01s01 = MakeMenuItem (72, 10, 80, 10, "Drive 2...", NULL, NULL);
		menu_Disk01s02 = MakeMenuItem (72, 20, 80, 10, "Drive 3...", NULL, NULL);
		menu_Disk01s03 = MakeMenuItem (72, 30, 80, 10, "Drive 4...", NULL, NULL);
		menu_Disk01s04 = MakeMenuItem (72, 40, 80, 10, "Drive 5...", NULL, NULL);
		menu_Disk01s05 = MakeMenuItem (72, 50, 80, 10, "Drive 6...", NULL, NULL);
		menu_Disk01s06 = MakeMenuItem (72, 60, 80, 10, "Drive 7...", NULL, NULL);
		menu_Disk01s07 = MakeMenuItem (72, 70, 80, 10, "Drive 8...", NULL, NULL);

		menu_Disk01s00->NextItem = menu_Disk01s01;
		menu_Disk01s01->NextItem = menu_Disk01s02;
		menu_Disk01s02->NextItem = menu_Disk01s03;
		menu_Disk01s03->NextItem = menu_Disk01s04;
		menu_Disk01s04->NextItem = menu_Disk01s05;
		menu_Disk01s05->NextItem = menu_Disk01s06;
		menu_Disk01s06->NextItem = menu_Disk01s07;

		menu_Disk01->SubItem = menu_Disk01s00;

	menu_Disk02 = MakeMenuItem (0, 20, 88, 10, "Eject disk", NULL, NULL);
		menu_Disk02s00 = MakeMenuItem (72, 0, 56, 10, "Drive 1", NULL, NULL);
		menu_Disk02s01 = MakeMenuItem (72, 10, 56, 10, "Drive 2", NULL, NULL);
		menu_Disk02s02 = MakeMenuItem (72, 20, 56, 10, "Drive 3", NULL, NULL);
		menu_Disk02s03 = MakeMenuItem (72, 30, 56, 10, "Drive 4", NULL, NULL);
		menu_Disk02s04 = MakeMenuItem (72, 40, 56, 10, "Drive 5", NULL, NULL);
		menu_Disk02s05 = MakeMenuItem (72, 50, 56, 10, "Drive 6", NULL, NULL);
		menu_Disk02s06 = MakeMenuItem (72, 60, 56, 10, "Drive 7", NULL, NULL);
		menu_Disk02s07 = MakeMenuItem (72, 70, 56, 10, "Drive 8", NULL, NULL);

		menu_Disk02s00->NextItem = menu_Disk02s01;
		menu_Disk02s01->NextItem = menu_Disk02s02;
		menu_Disk02s02->NextItem = menu_Disk02s03;
		menu_Disk02s03->NextItem = menu_Disk02s04;
		menu_Disk02s04->NextItem = menu_Disk02s05;
		menu_Disk02s05->NextItem = menu_Disk02s06;
		menu_Disk02s06->NextItem = menu_Disk02s07;

		menu_Disk02->SubItem = menu_Disk02s00;

	menu_Disk00->NextItem = menu_Disk01;
	menu_Disk01->NextItem = menu_Disk02;

	menu_Disk = MakeMenu (48, 0, 40, "Disk", menu_Disk00);

/*
 * Setup Console Menu
 */

	menu_Console00 = MakeMenuItem	(0, 0, 80, 10, "Option", NULL, NULL);
	menu_Console01 = MakeMenuItem	(0, 10, 80, 10, "Select", NULL, NULL);
	menu_Console02 = MakeMenuItem	(0, 20, 80, 10, "Start", NULL, NULL);
	menu_Console03 = MakeMenuItem	(0, 30, 80, 10, "Help", NULL, NULL);

		if (machine == Atari)
		{
			menu_Console03->Flags = ITEMTEXT | HIGHCOMP;
		}

	menu_Console04 = MakeMenuItem	(0, 45, 80, 10, "Reset", NULL, NULL);
	menu_Console05 = MakeMenuItem (0, 60, 80, 10, "Cold Start", NULL, NULL);

	menu_Console00->NextItem = menu_Console01;
	menu_Console01->NextItem = menu_Console02;
	menu_Console02->NextItem = menu_Console03;
	menu_Console03->NextItem = menu_Console04;
	menu_Console04->NextItem = menu_Console05;

	menu_Console = MakeMenu (88, 0, 64, "Console", menu_Console00);

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

	menu_Prefs00 = MakeMenuItem (0, 0, 96, 10, "Controller �", NULL, NULL);
		menu_Prefs00s00 = MakeMenuItem (80, 0, 80, 10, "Port 1...", NULL, NULL);
		menu_Prefs00s01 = MakeMenuItem (80, 10, 80, 10, "Port 2...", NULL, NULL);
			menu_Prefs00s01->Flags = ITEMTEXT | HIGHCOMP;

		menu_Prefs00s02 = MakeMenuItem (80, 20, 80, 10, "Port 3...", NULL, NULL);
/*
			if (machine != Atari)
			{
				menu_Prefs00s02->Flags = ITEMTEXT | HIGHCOMP;
			}
*/
			menu_Prefs00s02->Flags = ITEMTEXT | HIGHCOMP;

		menu_Prefs00s03 = MakeMenuItem (80, 30, 80, 10, "Port 4...", NULL, NULL);
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

	menu_Prefs01 = MakeMenuItem (0, 10, 96, 10, "Display    �", NULL, NULL);
		menu_Prefs01s00 = MakeMenuItem (80, 0, 64, 10, "  Colour", NULL, 2);
		menu_Prefs01s01 = MakeMenuItem (80, 10, 64, 10, "  Grey", NULL, 1);

		if (ColourEnabled)
		{
			menu_Prefs01s00->Flags = menu_Prefs01s00->Flags | CHECKED;
		}
		else
		{
			menu_Prefs01s01->Flags = menu_Prefs01s01->Flags | CHECKED;
		}

		menu_Prefs01s00->NextItem = menu_Prefs01s01;

		menu_Prefs01->SubItem = menu_Prefs01s00;

	menu_Prefs02 = MakeMenuItem (0, 20, 96, 10, "Refresh    �", NULL, NULL);
		menu_Prefs02s00 = MakeMenuItem (80, 0, 24, 10, "  1", NULL, 14);
		menu_Prefs02s01 = MakeMenuItem (80, 10, 24, 10, "  2", NULL, 13);
		menu_Prefs02s02 = MakeMenuItem (80, 20, 24, 10, "  4", NULL, 11);
		menu_Prefs02s03 = MakeMenuItem (80, 30, 24, 10, "  8", NULL, 7);

		if (refresh_rate == 1)
		{
			menu_Prefs02s00->Flags = menu_Prefs02s00->Flags | CHECKED;
		}
		else if (refresh_rate == 2)
		{
			menu_Prefs02s01->Flags = menu_Prefs02s01->Flags | CHECKED;
		}
		else if (refresh_rate == 4)
		{
			menu_Prefs02s02->Flags = menu_Prefs02s02->Flags | CHECKED;
		}
		else if (refresh_rate == 8)
		{
			menu_Prefs02s03->Flags = menu_Prefs02s03->Flags | CHECKED;
		}

		menu_Prefs02s00->NextItem = menu_Prefs02s01;
		menu_Prefs02s01->NextItem = menu_Prefs02s02;
		menu_Prefs02s02->NextItem = menu_Prefs02s03;

		menu_Prefs02->SubItem = menu_Prefs02s00;

	menu_Prefs03 = MakeMenuItem (0, 30, 96, 10, "Countdown  �", NULL, NULL);
		menu_Prefs03s00 = MakeMenuItem (80, 0, 56, 10, "  4000", NULL, 14);
		menu_Prefs03s01 = MakeMenuItem (80, 10, 56, 10, "  8000", NULL, 13);
		menu_Prefs03s02 = MakeMenuItem (80, 20, 56, 10, "  16000", NULL, 11);
		menu_Prefs03s03 = MakeMenuItem (80, 30, 56, 10, "  32000", NULL, 7);

		if (countdown_rate == 4000)
		{
			menu_Prefs03s00->Flags = menu_Prefs03s00->Flags | CHECKED;
		}
		else if (countdown_rate == 8000)
		{
			menu_Prefs03s01->Flags = menu_Prefs03s01->Flags | CHECKED;
		}
		else if (countdown_rate == 16000)
		{
			menu_Prefs03s02->Flags = menu_Prefs03s02->Flags | CHECKED;
		}
		else if (countdown_rate == 32000)
		{
			menu_Prefs03s03->Flags = menu_Prefs03s03->Flags | CHECKED;
		}

		menu_Prefs03s00->NextItem = menu_Prefs03s01;
		menu_Prefs03s01->NextItem = menu_Prefs03s02;
		menu_Prefs03s02->NextItem = menu_Prefs03s03;

		menu_Prefs03->SubItem = menu_Prefs03s00;

	menu_Prefs00->NextItem = menu_Prefs01;
	menu_Prefs01->NextItem = menu_Prefs02;
	menu_Prefs02->NextItem = menu_Prefs03;

	menu_Prefs = MakeMenu (152, 0, 48, "Prefs", menu_Prefs00);

/*
 * Link Menus
 */

	menu_Project->NextMenu = menu_Disk;
	menu_Disk->NextMenu = menu_Console;
	menu_Console->NextMenu = menu_Prefs;

	SetMenuStrip (WindowMain, menu_Project);

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

	image_data = (UBYTE*) malloc (ATARI_WIDTH * ATARI_HEIGHT);
	if (!image_data)
	{
		printf ("Failed to allocate space for image\n");
		Atari_Exit (0);
	}

	for (ptr=image_data,i=0;i<ATARI_WIDTH*ATARI_HEIGHT;i++,ptr++) *ptr = 0;
	for (ptr=(char *)data_Screen,i=0;i<ATARI_WIDTH*ATARI_HEIGHT;i++,ptr++) *ptr = 0;

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

int Atari_Exit (int run_monitor)
{
	if (run_monitor)
	{
		if (monitor ())
		{
			return TRUE;
		}
	}

	AbortIO (gameport_io_msg);

	while (GetMsg (gameport_msg_port))
	{
	}

	if (!gameport_error)
	{
		{
			BYTE type = GPCT_NOCONTROLLER;

			gameport_io_msg->io_Command = GPD_SETCTYPE;
			gameport_io_msg->io_Length = 1;
			gameport_io_msg->io_Data = (APTR) &type;

			DoIO (gameport_io_msg);

			if (gameport_io_msg->io_Error)
			{
				printf ("Failed to set controller type\n");
			}
		}

		CloseDevice (gameport_io_msg);
	}

	FreeMem (data_Screen, 46080 * 2);

	if (gameport_io_msg)
	{
		DeleteStdIO (gameport_io_msg);
	}

	if (gameport_msg_port)
	{
		DeletePort (gameport_msg_port);
	}

	if (WindowMain)
	{
		CloseWindow (WindowMain);
	}

	if (ScreenMain)
	{
		CloseScreen (ScreenMain);
	}

	if (LayersBase)
	{
		CloseLibrary (LayersBase);
	}

	if (GfxBase)
	{
		CloseLibrary (GfxBase);
	}

	if (IntuitionBase)
	{
		CloseLibrary (IntuitionBase);
	}

	exit (1);

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

void Atari_DisplayScreen (UBYTE *screen)
{
	int ypos;
	int	xpos;
	int	tbit = 15;
	UBYTE *scanline_ptr;

	BYTE pens[256];
	int pen;

	int colourmask;

	UWORD	*bitplane0;
	UWORD	*bitplane1;
	UWORD	*bitplane2;
	UWORD	*bitplane3;
	UWORD	*bitplane4;
	UWORD	*bitplane5;
	UWORD	*bitplane6;
	UWORD	*bitplane7;

	UWORD	word0, word1, word2, word3;
	UWORD	word4, word5, word6, word7;

	consol = 7;

	if (ChipSet == OCS_ChipSet || ChipSet == ECS_ChipSet)
	{
		for (pen=0;pen<256;pen++)
		{
			pens[pen] = 0;
		}

		pen = 5;

		if (CustomScreen)
		{
			pen = 1;
		}
	}

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

	if (ChipSet == AGA_ChipSet)
	{
		bitplane5 = &data_Screen[28800];
		word5 = *bitplane5;

		bitplane6 = &data_Screen[34560];
		word6 = *bitplane6;

		bitplane7 = &data_Screen[40320];
		word7 = *bitplane7;
	}

	scanline_ptr = image_data;

	if (ColourEnabled)
	{
		colourmask = 0xff;
	}
	else
	{
		colourmask = 0x0f;
	}

	if (ChipSet == AGA_ChipSet)
	{
		for (ypos=0;ypos<ATARI_HEIGHT;ypos++)
		{
			for (xpos=0;xpos<ATARI_WIDTH;xpos++)
			{
				UBYTE	colour;

				colour = *screen++ & colourmask;

				if (colour != *scanline_ptr)
				{
					UWORD mask;
					mask = ~(1 << tbit);

					word0 = (word0 & mask) | (((colour) & 1) << tbit);
					word1 = (word1 & mask) | (((colour >> 1) & 1) << tbit);
					word2 = (word2 & mask) | (((colour >> 2) & 1) << tbit);
					word3 = (word3 & mask) | (((colour >> 3) & 1) << tbit);
					word4 = (word4 & mask) | (((colour >> 4) & 1) << tbit);
					word5 = (word5 & mask) | (((colour >> 5) & 1) << tbit);
					word6 = (word6 & mask) | (((colour >> 6) & 1) << tbit);
					word7 = (word7 & mask) | (((colour >> 7) & 1) << tbit);

					*scanline_ptr++ = colour;
				}
				else
				{
					scanline_ptr++;
				}

				if (--tbit == -1)
				{
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

					*bitplane5++ = word5;
					word5 = *bitplane5;

					*bitplane6++ = word6;
					word6 = *bitplane6;

					*bitplane7++ = word7;
					word7 = *bitplane7;

					tbit = 15;
				}
			}
		}
	}
	else
	{
		if (ColourEnabled)
		{
			for (ypos=0;ypos<ATARI_HEIGHT;ypos++)
			{
				for (xpos=0;xpos<ATARI_WIDTH;xpos++)
				{
					UBYTE	colour;

					colour = *screen;

					if (pens[colour] == 0)
					{
						if (pen<33)
						{
							int rgb = colortable[colour];
							int red;
							int green;
							int blue;

							pens[colour] = pen;
							pen++;

							if (UpdatePalette)
							{
								red = (rgb & 0x00ff0000) >> 20;
								green = (rgb & 0x0000ff00) >> 12;
								blue = (rgb & 0x000000ff) >> 4;

								SetRGB4 (&WindowMain->WScreen->ViewPort, pen-1, red, green, blue);
							}
						}
					}

					colour = pens[*screen++];

					if (colour != *scanline_ptr)
					{
						UWORD mask;
						mask = ~(1 << tbit);

						word0 = (word0 & mask) | (((colour) & 1) << tbit);
						word1 = (word1 & mask) | (((colour >> 1) & 1) << tbit);
						word2 = (word2 & mask) | (((colour >> 2) & 1) << tbit);
						word3 = (word3 & mask) | (((colour >> 3) & 1) << tbit);
						word4 = (word4 & mask) | (((colour >> 4) & 1) << tbit);

						*scanline_ptr++ = colour;
					}
					else
					{
						scanline_ptr++;
					}

					if (--tbit == -1)
					{
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

						tbit = 15;
					}
				}
			}
		}
		else
		{
			for (ypos=0;ypos<ATARI_HEIGHT;ypos++)
			{
				for (xpos=0;xpos<ATARI_WIDTH;xpos++)
				{
					UBYTE	colour;

					colour = (*screen++ & 0x0f) + pen - 1;

					if (colour != *scanline_ptr)
					{
						UWORD mask;
						mask = ~(1 << tbit);

						word0 = (word0 & mask) | (((colour) & 1) << tbit);
						word1 = (word1 & mask) | (((colour >> 1) & 1) << tbit);
						word2 = (word2 & mask) | (((colour >> 2) & 1) << tbit);
						word3 = (word3 & mask) | (((colour >> 3) & 1) << tbit);
						word4 = (word4 & mask) | (((colour >> 4) & 1) << tbit);

						*scanline_ptr++ = colour;
					}
					else
					{
						scanline_ptr++;
					}

					if (--tbit == -1)
					{
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

						tbit = 15;
					}
				}
			}
		}
	}

	if (CustomScreen)
	{
		DrawImage (WindowMain->RPort, &image_Screen, -32, 0);
	}
	else
	{
		DrawImage (WindowMain->RPort, &image_Screen, 0, 0);
	}
}



/*
 * ==============
 * Atari_Keyboard
 * ==============
 */

/*
 * Revision     : v0.2.1a
 * Introduced   : NOT KNOWN (Before 2nd September 1995)
 * Last updated : 24th September 1995
 *
 * Notes: Currently contains GUI monitoring code as well. At some time in
 * the future I intend on removing this from this code.
 */

int Atari_Keyboard (void)
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

	while (IntuiMessage = (struct IntuiMessage*) GetMsg (WindowMain->UserPort))
	{
		Class = IntuiMessage->Class;
		Code = IntuiMessage->Code;
		Address = IntuiMessage->IAddress;
		MouseX = IntuiMessage->MouseX;
		MouseY = IntuiMessage->MouseY;

		MenuItem = Code;

		Menu = MENUNUM (MenuItem);
		Item = ITEMNUM (MenuItem);
		SubItem = SUBNUM (MenuItem);

		if (Class == MENUVERIFY)
		{
			if (ChipSet == OCS_ChipSet || ChipSet == ECS_ChipSet)
			{
				if (CustomScreen)
				{
					int i;

					UpdatePalette = FALSE;

					for (i=0;i<16;i++)
					{
						int rgb = colortable[i];
						int red;
						int green;
						int blue;

						red = (rgb & 0x00ff0000) >> 20;
						green = (rgb & 0x0000ff00) >> 12;
						blue = (rgb & 0x000000ff) >> 4;

						SetRGB4 (&WindowMain->WScreen->ViewPort, i, red, green, blue);
					}

					SetRGB4 (&WindowMain->WScreen->ViewPort, 24, 6, 8, 11);
				}
			}
		}

		ReplyMsg (IntuiMessage);

		switch (Class)
		{
			case VANILLAKEY :
				keycode = Code;

				switch (keycode)
				{
					case 0x01 :
						keycode = AKEY_CTRL_A;
						break;
					case 0x02 :
						keycode = AKEY_CTRL_B;
						break;
					case 0x03 :
						keycode = AKEY_CTRL_C;
						break;
					case 0x04 :
						keycode = AKEY_CTRL_D;
						break;
					case 0x05 :
						keycode = AKEY_CTRL_E;
						break;
					case 0x06 :
						keycode = AKEY_CTRL_F;
						break;
					case 0x07 :
						keycode = AKEY_CTRL_G;
						break;
/*
					case 0x08 :
						keycode = AKEY_CTRL_H;
						break;
*/
					case 0x09 :
						keycode = AKEY_CTRL_I;
						break;
					case 0x0a :
						keycode = AKEY_CTRL_J;
						break;
					case 0x0b :
						keycode = AKEY_CTRL_K;
						break;
					case 0x0c :
						keycode = AKEY_CTRL_L;
						break;
/*
					case 0x0d :
						keycode = AKEY_CTRL_M;
						break;
*/
					case 0x0e :
						keycode = AKEY_CTRL_N;
						break;
					case 0x0f :
						keycode = AKEY_CTRL_O;
						break;
					case 0x10 :
						keycode = AKEY_CTRL_P;
						break;
					case 0x11 :
						keycode = AKEY_CTRL_Q;
						break;
					case 0x12 :
						keycode = AKEY_CTRL_R;
						break;
					case 0x13 :
						keycode = AKEY_CTRL_S;
						break;
					case 0x14 :
						keycode = AKEY_CTRL_T;
						break;
					case 0x15 :
						keycode = AKEY_CTRL_U;
						break;
					case 0x16 :
						keycode = AKEY_CTRL_V;
						break;
					case 0x17 :
						keycode = AKEY_CTRL_W;
						break;
					case 0x18 :
						keycode = AKEY_CTRL_X;
						break;
					case 0x19 :
						keycode = AKEY_CTRL_Y;
						break;
					case 0x1a :
						keycode = AKEY_CTRL_Z;
						break;
					case 8 :
						keycode = AKEY_BACKSPACE;
						break;
					case 13 :
						keycode = AKEY_RETURN;
						break;
					case 0x1b :
						keycode = AKEY_ESCAPE;
						break;
					default :
						break;
				}
				break;
			case RAWKEY :
				switch (Code)
				{
					case 0x51 :
						consol &= 0x03;
						keycode = AKEY_NONE;
						break;
					case 0x52 :
						consol &= 0x05;
						keycode = AKEY_NONE;
						break;
					case 0x53 :
						consol &= 0x06;
						keycode = AKEY_NONE;
						break;
					case 0x55 :
						keycode = AKEY_PIL;
						break;
					case 0x56 :
						keycode = AKEY_BREAK;
						break;
					case 0x59 :
						keycode = AKEY_NONE;
						break;
					case 0x4f :
						keycode = AKEY_LEFT;
						break;
					case 0x4c :
						keycode = AKEY_UP;
						break;
					case 0x4e :
						keycode = AKEY_RIGHT;
						break;
					case 0x4d :
						keycode = AKEY_DOWN;
						break;
					default :
						break;
				}				
				break;
			case MOUSEBUTTONS :
				if (Controller == controller_Paddle)
				{
					switch (Code)
					{
						case SELECTDOWN :
							stick0 = 251;
							break;
						case SELECTUP :
							stick0 = 255;
							break;
						default :
							break;
					}
				}
				break;
			case MOUSEMOVE :
				if (Controller == controller_Paddle)
				{
					if (MouseX > 57)
					{
						if (MouseX < 287)
						{
							PaddlePos = 228 - (MouseX - 58);
						}
						else
						{
							PaddlePos = 0;
						}
					}
					else
					{
						PaddlePos = 228;
					}
				}
				break;
			case CLOSEWINDOW :
				if (DisplayYesNoWindow ())
				{
					keycode = AKEY_EXIT;
				}
				break;
			case MENUPICK :
				switch (Menu)
				{
					case 0 :
						switch (Item)
						{
							case 0 :
								DisplayNotSupportedWindow ();
								break;
							case 1 :
/*
								SystemTags
								(
									"Run >Nil: <Nil: MultiView Atari800.guide",
									SYS_Input, NULL,
									SYS_Output, NULL,
									TAG_DONE
								);
*/
								DisplayNotSupportedWindow ();
								break;
							case 2 :
								DisplayAboutWindow ();
								break;
							case 3 :
								if (DisplayYesNoWindow ())
								{
									keycode = AKEY_EXIT;
								}
								break;
							default :
								break;
						}
						break;
					case 1 :
						switch (Item)
						{
							case 0 :
								if (InsertDisk (1))
								{
									keycode = AKEY_COLDSTART;
								}
								break;
							case 1 :
								switch (SubItem)
								{
									case 0 :
										if (InsertDisk (1))
										{
										}
										break;
									case 1 :
										if (InsertDisk (2))
										{
										}
										break;
									case 2 :
										if (InsertDisk (3))
										{
										}
										break;
									case 3 :
										if (InsertDisk (4))
										{
										}
										break;
									case 4 :
										if (InsertDisk (5))
										{
										}
										break;
									case 5 :
										if (InsertDisk (6))
										{
										}
										break;
									case 6 :
										if (InsertDisk (7))
										{
										}
										break;
									case 7 :
										if (InsertDisk (8))
										{
										}
										break;
									default :
										break;
								}
								break;
							case 2 :
								switch (SubItem)
								{
									case 0 :
										SIO_Dismount (1);
										break;
									case 1 :
										SIO_Dismount (2);
										break;
									case 2 :
										SIO_Dismount (3);
										break;
									case 3 :
										SIO_Dismount (4);
										break;
									case 4 :
										SIO_Dismount (5);
										break;
									case 5 :
										SIO_Dismount (6);
										break;
									case 6 :
										SIO_Dismount (7);
										break;
									case 7 :
										SIO_Dismount (8);
										break;
									default :
										break;
								}
								break;
							default :
								break;
						}
						break;
					case 2 :
						switch (Item)
						{
							case 0 :
								consol &= 0x03;
								keycode = AKEY_NONE;
								break;
							case 1 :
								consol &= 0x05;
								keycode = AKEY_NONE;
								break;
							case 2 :
								consol &= 0x06;
								keycode = AKEY_NONE;
								break;
							case 3 :
								keycode = AKEY_HELP;
								break;
							case 4 :
								if (DisplayYesNoWindow ())
								{
									keycode = AKEY_WARMSTART;
								}
								break;
							case 5 :
								if (DisplayYesNoWindow ())
								{
									keycode = AKEY_COLDSTART;
								}
								break;
							default :
								break;
						}
						break;
					case 3 :
						switch (Item)
						{
							case 0 :
								switch (SubItem)
								{
									case 0 :
										DisplayControllerWindow (0);
										break;
									case 1 :
										DisplayControllerWindow (1);
										break;
									case 2 :
										DisplayControllerWindow (2);
										break;
									case 3 :
										DisplayControllerWindow (3);
										break;
									default :
										break;
								}
								break;
							case 1 :
								switch (SubItem)
								{
									case 0 :
										ColourEnabled = TRUE;
										break;
									case 1 :
										{
											int i;

											ColourEnabled = FALSE;

											for (i=0;i<16;i++)
											{
												int rgb = colortable[i];
												int red;
												int green;
												int blue;

												red = (rgb & 0x00ff0000) >> 20;
												green = (rgb & 0x0000ff00) >> 12;
												blue = (rgb & 0x000000ff) >> 4;

												if (CustomScreen)
												{
													SetRGB4 (&WindowMain->WScreen->ViewPort, i, red, green, blue);
												}
												else
												{
													SetRGB4 (&WindowMain->WScreen->ViewPort, i+4, red, green, blue);
												}
											}
										}
										break;
									default :
										break;
								}
								break;
							case 2 :
								switch (SubItem)
								{
									case 0 :
										refresh_rate = 1;
										break;
									case 1 :
										refresh_rate = 2;
										break;
									case 2 :
										refresh_rate = 4;
										break;
									case 3 :
										refresh_rate = 8;
										break;
									default :
										break;
								}
								break;
							case 3 :
								switch (SubItem)
								{
									case 0 :
										countdown_rate = 4000;
										break;
									case 1 :
										countdown_rate = 8000;
										break;
									case 2 :
										countdown_rate = 16000;
										break;
									case 3 :
										countdown_rate = 32000;
										break;
									default :
										break;
								}
								break;
							default :
								break;
						}
					default :
						break;
				}

				UpdatePalette = TRUE;

				break;
			default :
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

int Atari_Joystick ()
{
	WORD x;
	WORD y;
	UWORD code;

	int stick = 0;

	if (GetMsg (gameport_msg_port))
	{
		x = gameport_data.ie_X;
		y = gameport_data.ie_Y;
		code = gameport_data.ie_Code;

		switch (x)
		{
			case -1 :
				switch (y)
				{
					case -1 :
						stick = 10;
						break;
					case 0 :
						stick = 11;
						break;
					case 1 :
						stick = 9;
						break;
						default :
						break;
				}
				break;
			case 0 :
				switch (y)
				{
					case -1 :
						stick = 14;
						break;
					case 0 :
						stick = 15;
						break;
					case 1 :
						stick = 13;
						break;
					default :
						break;
				}
				break;
			case 1 :
				switch (y)
				{
					case -1 :
						stick = 6;
						break;
					case 0 :
						stick = 7;
						break;
					case 1 :
						stick = 5;
						break;
					default :
						break;
				}
				break;
			default :
				break;
		}

		if (code == IECODE_LBUTTON)
		{
			if (stick == 0)
			{
				stick = old_stick_0 & 15;
			}
			else
			{
				stick = stick & 15;
			}
		}

		if (code == IECODE_LBUTTON + IECODE_UP_PREFIX)
		{
			if (stick == 0)
			{
				stick = old_stick_0 | 16;
			}
			else
			{
				stick = stick | 16;
			}
		}

		old_stick_0 = stick;
		SendIO (gameport_io_msg);
	}
	
	if (stick == 0)
	{
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

int Atari_PORT (int num)
{
	if (num == 0)
	{
		if (Controller == controller_Joystick)
		{
			Atari_Joystick ();
			return 0xf0 | stick0;
		}
		else
		{
			return stick0;
		}
	}
	else
	{
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

int Atari_TRIG (int num)
{
	if (num == 0)
	{
		Atari_Joystick ();
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

int Atari_POT (int num)
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

int Atari_CONSOL (void)
{
	return consol;
}



/*
 * ==========
 * Atari_AUDC
 * ==========
 */

/*
 * Revision     : v0.1.9a
 * Introduced   : 2nd September 1995
 * Last updated : 2nd September 1995
 *
 * Notes: Currently acts as nothing more than a hook.
 */

void Atari_AUDC (int channel, int byte)
{
}



/*
 * ==========
 * Atari_AUDF
 * ==========
 */

/*
 * Revision     : v0.1.9a
 * Introduced   : 2nd September 1995
 * Last updated : 2nd September 1995
 *
 * Notes: Currently acts as nothing more than a hook.
 */

void Atari_AUDF (int channel, int byte)
{
}



/*
 * ============
 * Atari_AUDCTL
 * ============
 */

/*
 * Revision     : v0.1.9a
 * Introduced   : 2nd September 1995
 * Last updated : 2nd September 1995
 *
 * Notes: Currently acts as nothing more than a hook.
 */

void Atari_AUDCTL (int byte)
{
}



/*
 * ==================
 * DisplayAboutWindow
 * ==================
 */

/*
 * Revision     : v0.2.0
 * Introduced   : 2nd September 1995
 * Last updated : 9th September 1995
 */

void DisplayAboutWindow (void)

{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow	NewWindow;

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

	WindowAbout = (struct Window*) OpenWindowTags
	(
		&NewWindow,
		WA_NewLookMenus, TRUE,
		WA_MenuHelp, TRUE,
		WA_ScreenTitle, ATARI_TITLE,
		TAG_DONE
	);

	if (ScreenType == CUSTOMSCREEN)
	{
		SetRast (WindowAbout->RPort, 6);
	}

	Rule (0, 15, 288, WindowAbout);

	{
		char temp[128];
		char *ptr;

		strcpy (temp, ATARI_TITLE);
		ptr = strchr(temp,',');
		if (ptr)
		{
			int title_len;
			int version_len;

			*ptr++ = '\0';
			ptr++;

			title_len = strlen(temp);
			version_len = strlen(ptr);

			ShowText ((36 - title_len), 1, WindowAbout, temp);
			ShowText ((36 - version_len), 4, WindowAbout, ptr);
		}
		else
		{
			ShowText (19, 1, WindowAbout, "Atari800 Emulator");
			ShowText (21, 4, WindowAbout, "Version Unknown");
		}

		ShowText (17, 7, WindowAbout, "Original program by");
		ShowText (25, 9, WindowAbout, "David Firth");
		ShowText (18, 12, WindowAbout, "Amiga Module V5 by");
		ShowText (20, 14, WindowAbout, "Stephen A. Firth");
	}

	Gadget1 = MakeGadget (55, 17, 64, 14, 0, WindowAbout, "Ok", gadget_Button);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE)
	{
		while (IntuiMessage = (struct IntuiMessage*) GetMsg (WindowAbout->UserPort))
		{
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg (IntuiMessage);

			switch (Class)
			{
				case CLOSEWINDOW :
					QuitRoutine = TRUE;
					break;
				case GADGETUP :
					if (Address == (APTR) Gadget1)
					{
						QuitRoutine = TRUE;
					}
					break;
				default :
					break;
			}
		}
	}

	CloseWindow (WindowAbout);
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

void DisplaySetupWindow (void)

{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow	NewWindow;

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
	NewWindow.Height = 148; /* 177 */
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

	WindowSetup = (struct Window*) OpenWindowTags
	(
		&NewWindow,
		WA_NewLookMenus, TRUE,
		WA_MenuHelp, TRUE,
		WA_ScreenTitle, ATARI_TITLE,
		TAG_DONE
	);

	Rule (0, 15, 288, WindowSetup);

	ShowText (1, 1, WindowSetup, "Display:");

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
	Gadget1 = MakeGadget (3, 3, 16, 14, 0, WindowSetup, "AGA Screen", gadget_Mutual);
	Gadget1->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
	Gadget2 = MakeGadget (3, 5, 16, 14, 0, WindowSetup, "ECS Screen", gadget_Mutual);
	Gadget3 = MakeGadget (3, 7, 16, 14, 0, WindowSetup, "OCS Screen", gadget_Mutual);
	Gadget4 = MakeGadget (3, 9, 16, 14, 0, WindowSetup, "AGA Window", gadget_Mutual);
	Gadget5 = MakeGadget (3, 11, 16, 14, 0, WindowSetup, "ECS Window", gadget_Mutual);
	Gadget6 = MakeGadget (3, 13, 16, 14, 0, WindowSetup, "OCS Window", gadget_Mutual);
	Gadget7 = MakeGadget (55, 17, 64, 14, 0, WindowSetup, "Ok", gadget_Button);

	RefreshGadgets (Gadget1, WindowSetup, NULL);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE)
	{
		while (IntuiMessage = (struct IntuiMessage*) GetMsg (WindowSetup->UserPort))
		{
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg (IntuiMessage);

			switch (Class)
			{
				case CLOSEWINDOW :
					QuitRoutine = TRUE;
					break;
				case GADGETUP :
					{
						int NewFlagsForGadget1 = GADGIMAGE | GADGHIMAGE;
						int NewFlagsForGadget2 = GADGIMAGE | GADGHIMAGE;
						int NewFlagsForGadget3 = GADGIMAGE | GADGHIMAGE;
						int NewFlagsForGadget4 = GADGIMAGE | GADGHIMAGE;
						int NewFlagsForGadget5 = GADGIMAGE | GADGHIMAGE;
						int NewFlagsForGadget6 = GADGIMAGE | GADGHIMAGE;
						int NewFlags = FALSE;

						if (Address == (APTR) Gadget1)
						{
							ChipSet = AGA_ChipSet;
							CustomScreen = TRUE;
							NewFlagsForGadget1 = GADGIMAGE | GADGHIMAGE | SELECTED;
							NewFlags = TRUE;
						}
						else if (Address == (APTR) Gadget2)
						{
							ChipSet = ECS_ChipSet;
							CustomScreen = TRUE;
							NewFlagsForGadget2 = GADGIMAGE | GADGHIMAGE | SELECTED;
							NewFlags = TRUE;
						}
						else if (Address == (APTR) Gadget3)
						{
							ChipSet = OCS_ChipSet;
							CustomScreen = TRUE;
							NewFlagsForGadget3 = GADGIMAGE | GADGHIMAGE | SELECTED;
							NewFlags = TRUE;
						}
						else if (Address == (APTR) Gadget4)
						{
							/*
							 * NOTE: AGA Window Modes Currently Not Supported
							 */

							DisplayNotSupportedWindow ();

							Gadget4->Flags = GADGIMAGE | GADGHIMAGE;
							RefreshGadgets (Gadget1, WindowSetup, NULL);
/*
							QuitRoutine = TRUE;
							ChipSet = ECS_ChipSet;
							CustomScreen = FALSE;
*/
						}
						else if (Address == (APTR) Gadget5)
						{
							ChipSet = ECS_ChipSet;
							CustomScreen = FALSE;
							NewFlagsForGadget5 = GADGIMAGE | GADGHIMAGE | SELECTED;
							NewFlags = TRUE;
						}
						else if (Address == (APTR) Gadget6)
						{
							ChipSet = OCS_ChipSet;
							CustomScreen = FALSE;
							NewFlagsForGadget6 = GADGIMAGE | GADGHIMAGE | SELECTED;
							NewFlags = TRUE;
						}
						else if (Address == (APTR) Gadget7)
						{
							QuitRoutine = TRUE;
						}

						if (NewFlags)
						{
							Gadget1->Flags = NewFlagsForGadget1;
							Gadget2->Flags = NewFlagsForGadget2;
							Gadget3->Flags = NewFlagsForGadget3;
							Gadget4->Flags = NewFlagsForGadget4;
							Gadget5->Flags = NewFlagsForGadget5;
							Gadget6->Flags = NewFlagsForGadget6;
							RefreshGadgets (Gadget1, WindowSetup, NULL);
						}
					}
					break;
				default :
					break;
			}
		}
	}

	CloseWindow (WindowSetup);
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

void DisplayControllerWindow (int Port)

{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow	NewWindow;

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
	NewWindow.Height = 92; /* 177 */
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

	WindowController = (struct Window*) OpenWindowTags
	(
		&NewWindow,
		WA_NewLookMenus, TRUE,
		WA_MenuHelp, TRUE,
		WA_ScreenTitle, ATARI_TITLE,
		TAG_DONE
	);

	if (ScreenType == CUSTOMSCREEN)
	{
		SetRast (WindowController->RPort, 6);
	}

/* Re-insert for v0.2.2
	Rule (0, 16, 288, WindowController);
*/
	Rule (0, 7, 288, WindowController);

	ShowText (1, 1, WindowController, "Emulate:");

	Gadget1 = MakeGadget (3, 3, 16, 14, 0, WindowController, "Joystick", gadget_Mutual);

	if (Controller == controller_Joystick)
	{
		Gadget1->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
	}

	Gadget2 = MakeGadget (3, 5, 16, 14, 0, WindowController, "Paddle", gadget_Mutual);

	if (Controller == controller_Paddle)
	{
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

	Gadget8 = MakeGadget (39, 9, 64, 14, 0, WindowController, "Ok", gadget_Button);
	Gadget9 = MakeGadget (55, 9, 64, 14, 0, WindowController, "Cancel", gadget_Button);

	RefreshGadgets (Gadget1, WindowController, NULL);

	QuitRoutine = FALSE;
	Accepted = FALSE;

	NewController = Controller;

	while (QuitRoutine == FALSE)
	{
		while (IntuiMessage = (struct IntuiMessage*) GetMsg (WindowController->UserPort))
		{
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg (IntuiMessage);

			switch (Class)
			{
				case CLOSEWINDOW :
					QuitRoutine = TRUE;
					break;
				case GADGETUP :
					if (Address == (APTR) Gadget1)
					{
						Gadget1->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
						Gadget2->Flags = GADGIMAGE | GADGHIMAGE;
						RefreshGadgets (Gadget1, WindowController, NULL);
						NewController = controller_Joystick;
					}
					else if (Address == (APTR) Gadget2)
					{
						Gadget1->Flags = GADGIMAGE | GADGHIMAGE;
						Gadget2->Flags = GADGIMAGE | GADGHIMAGE | SELECTED;
						RefreshGadgets (Gadget1, WindowController, NULL);
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
					else if (Address == (APTR) Gadget8)
					{
						QuitRoutine = TRUE;
						Accepted = TRUE;
					}
					else if (Address == (APTR) Gadget9)
					{
						QuitRoutine = TRUE;
					}
					break;
				default :
					break;
			}
		}
	}

	if (Accepted)
	{
		Controller = NewController;
	}

	CloseWindow (WindowController);
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

void DisplayNotSupportedWindow (void)

{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow	NewWindow;

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

	WindowNotSupported = (struct Window*) OpenWindowTags
	(
		&NewWindow,
		WA_NewLookMenus, TRUE,
		WA_MenuHelp, TRUE,
		WA_ScreenTitle, ATARI_TITLE,
		TAG_DONE
	);

	if (ScreenType == CUSTOMSCREEN)
	{
		SetRast (WindowNotSupported->RPort, 6);
	}

	Rule (0, 2, 288, WindowNotSupported);

	ShowText (15, 1, WindowNotSupported, "Feature not supported");

	Gadget1 = MakeGadget (55, 4, 64, 14, 0, WindowNotSupported, "Ok", gadget_Button);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE)
	{
		while (IntuiMessage = (struct IntuiMessage*) GetMsg (WindowNotSupported->UserPort))
		{
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg (IntuiMessage);

			switch (Class)
			{
				case CLOSEWINDOW :
					QuitRoutine = TRUE;
					break;
				case GADGETUP :
					if (Address == (APTR) Gadget1)
					{
						QuitRoutine = TRUE;
					}
					break;
				default :
					break;
			}
		}
	}

	CloseWindow (WindowNotSupported);
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

int DisplayYesNoWindow (void)

{
	ULONG Class;
	USHORT Code;
	APTR Address;

	struct IntuiMessage *IntuiMessage;

	struct NewWindow	NewWindow;

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

	WindowYesNo = (struct Window*) OpenWindowTags
	(
		&NewWindow,
		WA_NewLookMenus, TRUE,
		WA_MenuHelp, TRUE,
		WA_ScreenTitle, ATARI_TITLE,
		TAG_DONE
	);

	if (ScreenType == CUSTOMSCREEN)
	{
		SetRast (WindowYesNo->RPort, 6);
	}

	Rule (0, 2, 288, WindowYesNo);

	ShowText (1, 1, WindowYesNo, "Are you sure?");

	Gadget1 = MakeGadget (39, 4, 64, 14, 0, WindowYesNo, "Ok", gadget_Button);
	Gadget2 = MakeGadget (55, 4, 64, 14, 0, WindowYesNo, "Cancel", gadget_Button);

	QuitRoutine = FALSE;

	while (QuitRoutine == FALSE)
	{
		while (IntuiMessage = (struct IntuiMessage*) GetMsg (WindowYesNo->UserPort))
		{
			Class = IntuiMessage->Class;
			Code = IntuiMessage->Code;
			Address = IntuiMessage->IAddress;

			ReplyMsg (IntuiMessage);

			switch (Class)
			{
				case CLOSEWINDOW :
					QuitRoutine = TRUE;
					break;
				case GADGETUP :
					if (Address == (APTR) Gadget1)
					{
						QuitRoutine = TRUE;
						Answer = TRUE;
					}
					else if (Address == (APTR) Gadget2)
					{
						QuitRoutine = TRUE;
					}
					break;
				default :
					break;
			}
		}
	}

	CloseWindow (WindowYesNo);

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

struct MenuItem *MakeMenuItem (int LeftEdge, int TopEdge, int Width, 
int Height, char *Title, int Key, int Exclude)

{
	struct IntuiText	*IntuiText;
	struct MenuItem		*MenuItem;

	IntuiText = (struct IntuiText*)malloc(sizeof(struct IntuiText));
	if (!IntuiText)
	{
	}

	IntuiText->FrontPen = 1;
	IntuiText->BackPen = 2;
	IntuiText->DrawMode = JAM1;
	IntuiText->LeftEdge = NULL;
	IntuiText->TopEdge = 1;
	IntuiText->ITextFont = NULL;
	IntuiText->IText = (UBYTE*)Title;
	IntuiText->NextText = NULL;

	MenuItem = (struct MenuItem*)malloc(sizeof(struct MenuItem));
	if (!MenuItem)
	{
	}

	MenuItem->NextItem = NULL;
	MenuItem->LeftEdge = LeftEdge;
	MenuItem->TopEdge = TopEdge;
	MenuItem->Width = Width;
	MenuItem->Height = Height;

	MenuItem->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;

	if (Key)
	{
		MenuItem->Flags = MenuItem->Flags | COMMSEQ;
	}

	if (Exclude)
	{
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
	MenuItem->ItemFill = (APTR)IntuiText;
	MenuItem->SelectFill = NULL;
	MenuItem->Command = (BYTE)Key;
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

struct Menu *MakeMenu (int LeftEdge, int TopEdge, int Width, char *Title, struct MenuItem *MenuItem)

{
	struct Menu *Menu;

	Menu = (struct Menu*)malloc(sizeof(struct Menu));
	if (!Menu)
	{
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

struct Gadget *MakeGadget (int LeftEdge, int TopEdge, int Width, int Height, int Offset, struct Window *Window, char *Title, int Type)

{
	int Result;
	int TextX;

	struct IntuiText *NewTextAddress = NULL;
	struct Gadget *NewGadgetAddress = NULL;

	NewGadgetAddress = (struct Gadget*)malloc(sizeof(struct Gadget));
	if (!NewGadgetAddress)
	{
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
	if (Type == gadget_Button)
	{
		NewGadgetAddress->GadgetRender = &image_Button64;
		NewGadgetAddress->SelectRender = &image_Button64Selected;
	}
	else if (Type == gadget_Mutual)
	{
		NewGadgetAddress->Activation = TOGGLESELECT | RELVERIFY;
		NewGadgetAddress->GadgetRender = &image_MutualGadget;
		NewGadgetAddress->SelectRender = &image_MutualGadgetSelected;
	}
	else
	{
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

	if (Type == gadget_Button)
	{
		TextX=(Width/2)-(strlen(Title)*4);
	}
	else if (Type == gadget_Mutual)
	{
		TextX = 24;
	}

	NewTextAddress = (struct IntuiText*)malloc(sizeof(struct IntuiText));
	if (!NewTextAddress)
	{
	}

	NewTextAddress->FrontPen = 1;
	NewTextAddress->BackPen = 3;
	NewTextAddress->DrawMode = JAM1;
	NewTextAddress->LeftEdge = TextX;
	NewTextAddress->TopEdge = 3;
	NewTextAddress->ITextFont = NULL;
	NewTextAddress->IText = (UBYTE*)Title;
	NewTextAddress->NextText = NULL;

	NewGadgetAddress->GadgetText = NewTextAddress;

	if (NewGadgetAddress->Activation & TOGGLESELECT)
	{
		if (Title)
		{
			if (Offset)
			{
				NewTextAddress->LeftEdge = Offset;
			}
		}
	}

	if (Window)
	{
		Result = AddGadget (Window, NewGadgetAddress, -1);
		RefreshGadgets (NewGadgetAddress, Window, NULL);
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

void ShowText (int LeftEdge, int TopEdge, struct Window *Window, char *TextString)

{
	if (ScreenType == CUSTOMSCREEN)
	{
		SetAPen (Window->RPort, 0);
		SetBPen (Window->RPort, 6);
	}
	else
	{
		SetAPen (Window->RPort, 1);
		SetBPen (Window->RPort, 0);
	}

	LeftEdge = (LeftEdge * gui_GridWidth);
	TopEdge = (TopEdge * gui_GridHeight) + ((gui_GridHeight - 7) * 2);

	if (Window->Flags & GIMMEZEROZERO)
	{
		TopEdge = TopEdge + 8;
	}
	else
	{
		LeftEdge = LeftEdge + gui_WindowOffsetLeftEdge;
		TopEdge = TopEdge + gui_WindowOffsetTopEdge + 8;
	}

	Move (Window->RPort, LeftEdge, TopEdge);
	Text (Window->RPort, TextString, strlen(TextString));
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

void Rule (int LeftEdge, int TopEdge, int Width, struct Window *Window)

{
	int RightEdge;

	LeftEdge = (LeftEdge * gui_GridWidth) + gui_WindowOffsetLeftEdge;
	TopEdge = (TopEdge * gui_GridHeight) + 12 + gui_WindowOffsetTopEdge; /* + 44 */

	RightEdge = Width + LeftEdge - 1;

	if (ScreenType == CUSTOMSCREEN)
	{
		SetAPen (Window->RPort, 0);
	}
	else
	{
		SetAPen (Window->RPort, 1);
	}

	Move (Window->RPort, LeftEdge, TopEdge);
	Draw (Window->RPort, RightEdge, TopEdge);

	if (ScreenType == CUSTOMSCREEN)
	{
		SetAPen (Window->RPort, 15);
	}
	else
	{
		SetAPen (Window->RPort, 2);
	}

	Move (Window->RPort, LeftEdge, TopEdge + 1);
	Draw (Window->RPort, RightEdge, TopEdge + 1);
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

int InsertDisk (int Drive)

{
	struct FileRequester *FileRequester = NULL;
	char Filename[256];
	int Success = FALSE;

	if (FileRequester = (struct FileRequester*) AllocAslRequestTags (ASL_FileRequest, TAG_DONE))
	{
		if (AslRequestTags (FileRequester,
			ASLFR_Screen, ScreenMain,
			ASLFR_TitleText, "Select file",
			ASLFR_PositiveText, "Ok",
			ASLFR_NegativeText, "Cancel",
			TAG_DONE))
		{
			printf ("File selected: %s/%s\n", FileRequester->rf_Dir, FileRequester->rf_File);
			printf ("Number of files : %d\n", FileRequester->fr_NumArgs);

			SIO_Dismount (Drive);

			sprintf (Filename, "%s/%s", FileRequester->rf_Dir, FileRequester->rf_File);

			if (!SIO_Mount (Drive, Filename))
			{
			}
			else
			{
				Success = TRUE;
			}
		}
		else
		{
			/*
			 * Cancelled
			 */
		}
	}
	else
	{
		printf ("Unable to create requester\n");
	}

	return Success;
}
