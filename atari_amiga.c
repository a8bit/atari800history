#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>

#include <devices/ahi.h>
#include <devices/gameport.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <cybergraphx/cybergraphics.h>
#include <cybergraphx/cgxvideo.h>
#include <workbench/workbench.h>

#include <proto/exec.h>
#include <proto/asl.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/gadtools.h>
#include <proto/timer.h>

#include <clib/ahi_protos.h>
#include <clib/alib_protos.h>
#include <clib/asyncio_protos.h>

#define IEQUALIFIER_SHIFT (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)

#ifndef __MAXON__

#include <clib/cybergraphics_protos.h>
#include <clib/cgxvideo_protos.h>
#include <clib/muimaster_protos.h>

#include <pragmas/muimaster_pragmas.h>
#include <pragmas/cybergraphics_pragmas.h>
#include <pragmas/cgxvideo_pragmas.h>

#define ASM __asm
#define SAVEDS __saveds

__near LONG __stack = 150000;

#else

#include <pragma/ahi_lib.h>
#include <pragma/muimaster_lib.h>
#include <pragma/cybergraphics_lib.h>
#include <pragma/cgxvideo_lib.h>

ULONG AttachVLayerTags(struct VLayerHandle *vl, struct Window *wnd, Tag tag, ...)
{
	return AttachVLayerTagList(vl,wnd,(TagItem*)&tag);
}

#define ASM
#define SAVEDS

void Aprint(char *format, ... ){}

#endif

#include "config.h"

#include "atari.h"
#include "binload.h"
#include "colours.h"
#include "monitor.h"
#include "pokeysnd.h"
#include "sio.h"
#include "statesav.h"
#include "rt-config.h"

#include "atari_amiga.h"
#include "amiga/amiga_asm.h"

extern int refresh_rate;

#define GAMEPORT0 0
#define GAMEPORT1 1

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *LayersBase;
struct Library *AslBase;
struct Device *TimerBase;
struct Library *AHIBase;
struct Library *MUIMasterBase;
struct Library *GadToolsBase;
struct Library *CGXVideoBase;
struct Library *CyberGfxBase;

/* Gameport */

struct InputEvent gameport_data;
struct MsgPort *gameport_msg_port;
struct IOStdReq *gameport_io_msg;
BOOL gameport_error;

/* Timer */

struct MsgPort *timer_msg_port;
struct timerequest *timer_request;
BOOL timer_error;

/* Sound */

struct MsgPort *ahi_msg_port;
struct AHIRequest *ahi_request;
struct AHIRequest *ahi_soundreq[2];
STRPTR ahi_soundpath;
AsyncFile *ahi_soundhandle;	/* FileHandle for Soundoutput */
BOOL ahi_error;
BOOL ahi_current;

UWORD ahi_fps;                  /* frames per second */
UWORD ahi_rupf;                 /* real updates per frame */
BYTE *ahi_streambuf[2];
ULONG ahi_streamfreq = 22050;   /* playing frequence */
ULONG ahi_streamlen;
ULONG ahi_samplepos;
ULONG ahi_updatecount;

/* MUI GUI */

Object *app;

Object *settings_wnd;
Object *settings_model_cycle;
Object *settings_ram_check;
Object *settings_patch_check;
Object *settings_holdoption_check;
Object *settings_sound_check;
Object *settings_skipframe_slider;
Object *settings_screentype_cycle;
Object *settings_screenmode_text;
Object *settings_screenmode_popup;
Object *settings_best_check;
Object *settings_overlay_check;
Object *settings_scalable_check;
Object *settings_save_button;
Object *settings_use_button;
Object *settings_cancel_button;
Object *settings_osa_string;
Object *settings_osb_string;
Object *settings_xlxe_string;
Object *settings_basic_string;
Object *settings_cartriges_string;
Object *settings_disks_string;
Object *settings_exes_string;
Object *settings_states_string;
		
struct Hook settings_screentype_hook;
struct Hook settings_screenmode_starthook;
struct Hook settings_screenmode_stophook;

/* Emulation */

int old_stick_0 = 0x1f;
int old_stick_1 = 0x1f;

static int consol;
static int trig0;
static int stick0;

static LONG ScreenType;
static LONG Overlay;
static LONG Scalable;
static LONG UseBestID=TRUE;
static LONG SoundEnabled=TRUE;
static LONG ShowFPS;
static ULONG DisplayID=INVALID_ID;
static STRPTR DiskPath;
static LONG DiskPathSet;

struct Screen *ScreenMain;
APTR VisualInfoMain;
struct Window *WindowMain = NULL;
struct Menu *MenuMain;
APTR VLHandle;
BOOL VLAttached;

UWORD *colortable15;
UBYTE *colortable8;
UBYTE *tempscreendata;

struct FileRequester *DiskFileReq;
struct FileRequester *CartFileReq;
struct FileRequester *StateFileReq;
struct FileRequester *BinFileReq;

static int Controller;
static int PaddlePos;

#define controller_Null 0
#define controller_Joystick 1
#define controller_Paddle 2

#define InnerWidth(w) (w->Width - w->BorderLeft - w->BorderRight)
#define InnerHeight(w) (w->Height - w->BorderTop - w->BorderBottom)

/**************************************************************************
 usleep() function. (because select() is too slow and not needed to be
 emulated for the client). Complete.
**************************************************************************/
void usleep(unsigned long usec)
{
	timer_request->tr_time.tv_secs = 0;
	timer_request->tr_time.tv_micro = usec;
	DoIO((struct IORequest*)timer_request);
}

//-------------------------------------
// Menü

enum{
	MEN_PROJECT=1,
	MEN_PROJECT_ABOUT,
	MEN_PROJECT_LOADSTATE,
	MEN_PROJECT_SAVESTATE,
	MEN_PROJECT_LOADBIN,
	MEN_PROJECT_RECORDSOUND,
	MEN_PROJECT_SOUNDPATH,
	MEN_PROJECT_ICONIFY,
	MEN_PROJECT_HELP,
	MEN_PROJECT_QUIT,

	MEN_SYSTEM,
	MEN_SYSTEM_BOOT,
	MEN_SYSTEM_ID,
	MEN_SYSTEM_ID1,
	MEN_SYSTEM_ID2,
	MEN_SYSTEM_ID3,
	MEN_SYSTEM_ID4,
	MEN_SYSTEM_ID5,
	MEN_SYSTEM_ID6,
	MEN_SYSTEM_ID7,
	MEN_SYSTEM_ID8,
	MEN_SYSTEM_ED,
	MEN_SYSTEM_ED1,
	MEN_SYSTEM_ED2,
	MEN_SYSTEM_ED3,
	MEN_SYSTEM_ED4,
	MEN_SYSTEM_ED5,
	MEN_SYSTEM_ED6,
	MEN_SYSTEM_ED7,
	MEN_SYSTEM_ED8,
	MEN_SYSTEM_IC,
	MEN_SYSTEM_IC8K,
	MEN_SYSTEM_IC16K,
	MEN_SYSTEM_ICOSS,
	MEN_SYSTEM_REMOVEC,
	MEN_SYSTEM_PILL,
	MEN_SYSTEM_ATARI800A,
	MEN_SYSTEM_ATARI800B,
	MEN_SYSTEM_ATARI800XL,
	MEN_SYSTEM_ATARI130XL,
	MEN_SYSTEM_UI,

	MEN_CONSOLE,
	MEN_CONSOLE_RESET,
	MEN_CONSOLE_OPTION,
	MEN_CONSOLE_SELECT,
	MEN_CONSOLE_START,
	MEN_CONSOLE_HELP,
	MEN_CONSOLE_BREAK,
	MEN_CONSOLE_COLDSTART,

	MEN_SETTINGS,
	MEN_SETTINGS_FRAMERATE
};

static struct NewMenu MenuEntries[] =
{
	NM_TITLE, "Project", NULL, 0, 0L, (APTR)MEN_PROJECT,
	NM_ITEM, "About...", "?", 0, 0L, (APTR)MEN_PROJECT_ABOUT,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Load State...", "S", 0, 0L, (APTR)MEN_PROJECT_LOADSTATE,
	NM_ITEM, "Save State...", "L", 0, 0L, (APTR)MEN_PROJECT_SAVESTATE,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Load BIN...", NULL, 0, 0L, (APTR)MEN_PROJECT_LOADBIN,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Record Sound?", NULL, MENUTOGGLE|CHECKIT, 0L, (APTR)MEN_PROJECT_RECORDSOUND,
	NM_ITEM, "Select Path...", NULL, 0, 0L, (APTR)MEN_PROJECT_SOUNDPATH,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Help...", NULL, 0, 0L, (APTR)MEN_PROJECT_HELP,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Iconify", NULL, 0, 0L, (APTR)MEN_PROJECT_ICONIFY,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Quit...", "Q", 0, 0L, (APTR)MEN_PROJECT_QUIT,
	NM_TITLE, "System", NULL, 0, 0L, (APTR)MEN_SYSTEM,
	NM_ITEM, "Boot Disk...", "B", 0, 0L, (APTR)MEN_SYSTEM_BOOT,
	NM_ITEM, "Insert Disk", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID,
	NM_SUB, "Drive 1...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID1,
	NM_SUB, "Drive 2...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID2,
	NM_SUB, "Drive 3...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID3,
	NM_SUB, "Drive 4...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID4,
	NM_SUB, "Drive 5...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID5,
	NM_SUB, "Drive 6...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID6,
	NM_SUB, "Drive 7...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID7,
	NM_SUB, "Drive 8...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID8,
	NM_ITEM, "Eject Disk", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED,
	NM_SUB, "Drive 1", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED1,
	NM_SUB, "Drive 2", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED2,
	NM_SUB, "Drive 3", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED3,
	NM_SUB, "Drive 4", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED4,
	NM_SUB, "Drive 5", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED5,
	NM_SUB, "Drive 6", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED6,
	NM_SUB, "Drive 7", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED7,
	NM_SUB, "Drive 8", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED8,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Insert Cartride", NULL, 0, 0L, (APTR)MEN_SYSTEM_IC,
	NM_SUB, "8K Cart...", NULL, 0, 0L, (APTR)MEN_SYSTEM_IC8K,
	NM_SUB, "16K Cart...", NULL, 0, 0L, (APTR)MEN_SYSTEM_IC16K,
	NM_SUB, "OSS SuperCart", NULL, 0, 0L, (APTR)MEN_SYSTEM_ICOSS,
	NM_ITEM, "Remove Cartride", NULL, 0, 0L, (APTR)MEN_SYSTEM_REMOVEC,
	NM_ITEM, "Enable PILL", "F6", NM_COMMANDSTRING, 0L, (APTR)MEN_SYSTEM_PILL,
	NM_ITEM, NM_BARLABEL, NULL,   0, 0L, NULL,
	NM_ITEM, "Internal User Interface", NULL, 0, 0L, (APTR)MEN_SYSTEM_UI,
/*	NM_ITEM, "Atari 800 OS/A", NULL, 0, 0L, (APTR)NULL,
	NM_ITEM, "Atari 800 OS/B", NULL, 0, 0L, NULL,
	NM_ITEM, "Atari 800XL", NULL, 0, 0L, NULL,
	NM_ITEM, "Atari 130XE", NULL, 0, 0L, NULL,*/
	NM_TITLE, "Console", NULL, 0, 0L, (APTR)MEN_CONSOLE,
	NM_ITEM, "Option", "F2", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_OPTION,
	NM_ITEM, "Select", "F3", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_SELECT,
	NM_ITEM, "Start", "F4", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_START,
	NM_ITEM, "Help", "Help", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_HELP,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Break", "F7", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_BREAK,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Reset", "F5", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_RESET,
	NM_ITEM, "Coldstart", "Shift F5", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_COLDSTART,
	NM_TITLE, "Settings", NULL, 0, 0L, (APTR)MEN_SETTINGS,
	NM_ITEM, "Show Framerate?", NULL, MENUTOGGLE|CHECKIT, 0L, (APTR)MEN_SETTINGS_FRAMERATE,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Edit Settings...", NULL, 0, 0L, NULL,
	NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL,
	NM_ITEM, "Save Settings", NULL, 0, 0L, NULL,
	NM_END, NULL, NULL, 0, 0L, NULL
};
//-------------------------------------
// MUI Support

#define GUI_SAVE		1
#define GUI_USE		2
#define GUI_CANCEL	MUIV_Application_ReturnID_Quit

//#define get(obj,attr,store) GetAttr(attr,obj,(ULONG *)store)
//#define set(obj,attr,value) SetAttrs(obj,attr,value,TAG_DONE)
//#define nnset(obj,attr,value) SetAttrs(obj,MUIA_NoNotify,TRUE,attr,value,TAG_DONE)

/*#define setmutex(obj,n)     set(obj,MUIA_Radio_Active,n)
#define setcycle(obj,n)     set(obj,MUIA_Cycle_Active,n)
#define setstring(obj,s)    set(obj,MUIA_String_Contents,s)
*/
#define nnsetstring(obj,s)  nnset(obj,MUIA_String_Contents,s)
/*#define setcheckmark(obj,b) set(obj,MUIA_Selected,b)
#define setslider(obj,l)    set(obj,MUIA_Numeric_Value,l)
*/

LONG xget(Object *obj,ULONG attribute)
{
	LONG x;
	get(obj,attribute,&x);
	return(x);
}

char *getstr(Object *obj)
{
	return((char *)xget(obj,MUIA_String_Contents));
}

BOOL getbool(Object *obj)
{
	return((BOOL)xget(obj,MUIA_Selected));
}

Object *MUI_NewObject          (char *classname,Tag tag1,...)
{
	return MUI_NewObjectA(classname, (struct TagItem*)&tag1);
}

BOOL MUI_AslRequestTags(APTR requester, Tag Tag1, ...)
{
	return MUI_AslRequest(requester, (struct TagItem*)&Tag1);
}

Object *MUI_MakeObject         (LONG type,...)
{
	return MUI_MakeObjectA(type, ((ULONG*)&type)+1);
}

LONG    MUI_Request            (APTR app,APTR win,LONGBITS flags,char *title,char *gadgets,char *format,...)
{
	return MUI_RequestA(app,win,flags,title,gadgets,format,((ULONG*)&format)+1);
}


Object *MakeLabel(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,0)); }
Object *MakeLabel1(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_SingleFrame)); }
Object *MakeLabel2(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_DoubleFrame)); }
Object *MakeLLabel(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_LeftAligned)); }
Object *MakeLLabel1(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_LeftAligned|MUIO_Label_SingleFrame)); }

Object *MakeCheck(BOOL check)
{
	Object *obj = MUI_MakeObject(MUIO_Checkmark,NULL);
	if (obj) set(obj,MUIA_CycleChain,1);
	return(obj);
}

Object *MakeButton(STRPTR str)
{
	Object *obj = MUI_MakeObject(MUIO_Button,str);
	if (obj) set(obj,MUIA_CycleChain,1);
	return(obj);
}

Object *MakeCycle(STRPTR *array)
{
	Object *obj = MUI_MakeObject(MUIO_Cycle, NULL, array);
	if (obj) set(obj,MUIA_CycleChain,1);
	return(obj);
}

Object *MakeString(STRPTR def, LONG maxlen)
{
	Object *obj = MUI_MakeObject(MUIO_String,NULL,maxlen);

	if (obj)
	{
		SetAttrs(obj,
			MUIA_CycleChain,1,
			MUIA_String_Contents,def,
//			MUIA_String_AdvanceOnCR,TRUE,
			TAG_DONE);
	}

	return(obj);
}

/*Object *MakeInteger(LONG num)
{
	Object *obj = MUI_MakeObject(MUIO_String,NULL,10);

	if (obj)
	{
		SetAttrs(obj,
			MUIA_CycleChain,1,
  		MUIA_String_Accept, "0123456789",
  		MUIA_String_Integer,0,//num,
			TAG_DONE);
	}

	return(obj);
}*/

Object *MakeImageButton(ULONG image)
{
	return ImageObject,ImageButtonFrame,
								MUIA_Image_Spec,image,
								MUIA_Background, MUII_ButtonBack,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
								MUIA_Image_FreeVert, TRUE,
								MUIA_CycleChain,1,
								End;

}

Object *MakeLV(APTR pool)
{
	return(
		ListviewObject,
			MUIA_Listview_Input, TRUE,
			MUIA_Listview_List , ListObject,
				InputListFrame,
				MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
				MUIA_List_DestructHook , MUIV_List_DestructHook_String,
				pool?MUIA_List_Pool:TAG_IGNORE,pool,
				End,
			End
	);
}

ULONG DoSuperNew(struct IClass *cl,Object *obj,ULONG tag1,...)
{
	return(DoSuperMethod(cl,obj,OM_NEW,&tag1,NULL));
}

//-------------------------------------

//-------------------------------------
LONG StrLen( const STRPTR str)
{
	if(str) return (LONG)strlen(str);
	return 0;
}
//-------------------------------------
STRPTR StrCopy( const STRPTR str )
{
	STRPTR dst;
	if( !str ) return NULL;
	if( !*str) return NULL;

	dst = (STRPTR)AllocVec(strlen(str)+1,0);
	if(dst) strcpy(dst,str);
	return dst;
}
//-------------------------------------
STRPTR GetFullPath( STRPTR drw, STRPTR file)
{
	WORD dl = StrLen(drw);
	WORD fl = StrLen( file );
	LONG length = dl + fl + 6;
	STRPTR fp = (STRPTR)AllocVec( length+1, 0 );

	if( fp )
	{
		strcpy( fp, drw );

		if( AddPart( fp, file, length ))	return fp;
		else FreeVec( fp );
	}
	return NULL;
}
//-------------------------------------
STRPTR AddSuffix(const STRPTR name, const STRPTR suf)
{
	STRPTR str;
	if(!strstr(name,suf))
	{
		LONG len = StrLen(name)+StrLen(suf)+2;
		str = (STRPTR)AllocVec(len,0);
		if(str)
		{
			strcpy(str,name);
			strcat(str,suf);
		}
	}	else str = StrCopy(name);
	return str;
}
//-------------------------------------
ULONG GetBestID( ULONG width, ULONG height, ULONG depth )
{
	struct Screen *defscr = LockPubScreen( NULL );
	struct ViewPort *vp = NULL;
	ULONG displayID;
	if( defscr ) vp = &defscr->ViewPort;

	displayID = BestModeID( BIDTAG_Depth,depth,
									BIDTAG_NominalWidth, width,
									BIDTAG_NominalHeight, height,
									BIDTAG_MonitorID, GetVPModeID( vp ) & MONITOR_ID_MASK,
									TAG_DONE);

	if( displayID == INVALID_ID )
	{
		displayID = BestModeID( BIDTAG_Depth,depth,
									BIDTAG_NominalWidth, width,
									BIDTAG_NominalHeight, height,
									TAG_DONE);
	}

	

	if( defscr ) UnlockPubScreen( NULL, defscr );
	return displayID;
}
//-------------------------------------
STRPTR GetDisplayName(ULONG displayid)
{
	STATIC struct NameInfo DisplayNameInfo;
	STATIC char DisplayNameBuffer[256];

	LONG i, v;

	i	= 0;
	v	= GetDisplayInfoData(NULL,	(UBYTE *) &DisplayNameInfo, sizeof(DisplayNameInfo),
									DTAG_NAME, displayid);

	if(v > sizeof(struct QueryHeader))
    {
		for(; (i < sizeof(DisplayNameBuffer) - 1) && DisplayNameInfo.Name[i]; i++)
			DisplayNameBuffer[i]	= DisplayNameInfo.Name[i];
	}

	if(displayid == INVALID_ID)
		strcpy(DisplayNameBuffer, "InvalidID"/*GetMessage(MSG_INVALID)*/);
	else
	{
		if(i < sizeof(DisplayNameBuffer) - sizeof(" (0x00000000)"))
		{
			DisplayNameBuffer[i++]	= ' ';
			DisplayNameBuffer[i++]	= '(';
			DisplayNameBuffer[i++]	= '0';
			DisplayNameBuffer[i++]	= 'x';

			for(v = 28; (v >= 0) && (!((displayid >> v) & 0xf)); v -= 4);

			if(v < 0)
				DisplayNameBuffer[i++]	= '0';

			for(; (v >= 0); v -= 4)
			{
				if(((displayid >> v) & 0xf) > 9)
					DisplayNameBuffer[i++]	= ((displayid >> v) & 0xf) + 'a' - 10;
				else
					DisplayNameBuffer[i++]	= ((displayid >> v) & 0xf) + '0';
			}
			DisplayNameBuffer[i++]	= ')';
		}

		DisplayNameBuffer[i++]	= 0;
	}

	return DisplayNameBuffer;
//	set(DisplayName, MUIA_Text_Contents, (ULONG) DisplayNameBuffer);
}
//-------------------------------------
APTR FindUserData( struct Menu *menu, APTR userdata)
{
	while(menu)
	{
		struct MenuItem *mi;

		if(GTMENU_USERDATA( menu ) == userdata) return menu;

		mi = menu->FirstItem;
		while(mi)
		{
			struct MenuItem *smi;

			if(GTMENUITEM_USERDATA( mi ) == userdata) return mi;

			smi = mi->SubItem;
			while(smi)
			{
				if(GTMENUITEM_USERDATA( smi ) == userdata) return smi;
				smi = smi->NextItem;
			}
			mi = mi->NextItem;
		}
		menu = menu->NextMenu;
	}
	return NULL;
}
//-------------------------------------

//-------------------------------------
VOID CloseLibraries(void)
{
	if( CyberGfxBase ) CloseLibrary(CyberGfxBase);
	if( CGXVideoBase ) CloseLibrary(CGXVideoBase);
	if( GadToolsBase ) CloseLibrary(GadToolsBase);
	if( AslBase ) CloseLibrary(AslBase);
	if( LayersBase ) CloseLibrary(LayersBase);
	if( GfxBase ) CloseLibrary((struct Library*)GfxBase);
	if( IntuitionBase ) CloseLibrary((struct Library*)IntuitionBase);
}
//-------------------------------------
BOOL OpenLibraries(void)
{
	if((IntuitionBase = (struct IntuitionBase*)OpenLibrary ("intuition.library", 39)))
	{
		if((GfxBase = (struct GfxBase*)OpenLibrary ("graphics.library", 39)))
		{
			if((LayersBase = OpenLibrary ("layers.library", 39)))
			{
				if((AslBase = OpenLibrary ("asl.library", 39)))
				{
					if((GadToolsBase = OpenLibrary("gadtools.library",39)))
					{
						CGXVideoBase = OpenLibrary("cgxvideo.library",41);
						CyberGfxBase=OpenLibrary("cybergraphics.library",41);

						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}
//-------------------------------------
VOID FreeJoystick(void)
{
	if( gameport_io_msg )
	{
		if( !gameport_error)
		{
			BYTE type = GPCT_NOCONTROLLER;

			AbortIO((struct IORequest*)gameport_io_msg);
			WaitIO((struct IORequest*)gameport_io_msg);

			gameport_io_msg->io_Command = GPD_SETCTYPE;
			gameport_io_msg->io_Length = 1;
			gameport_io_msg->io_Data = (APTR) &type;
			DoIO ((struct IORequest*)gameport_io_msg);

			CloseDevice((struct IORequest*)gameport_io_msg);
		}

		DeleteIORequest(gameport_io_msg);
		gameport_io_msg = NULL;
	}
	if( gameport_msg_port )
	{
		DeleteMsgPort(gameport_msg_port);
		gameport_msg_port = NULL;
	}
}
//-------------------------------------
BOOL SetupJoystick(void)
{
	if((gameport_msg_port = CreateMsgPort()))
	{
		gameport_io_msg = (struct IOStdReq *) CreateIORequest (gameport_msg_port, sizeof(struct IOStdReq));
		if( gameport_io_msg )
		{
			gameport_error = OpenDevice ("gameport.device", GAMEPORT1, (struct IORequest*)gameport_io_msg, 0xFFFF);
			if(!gameport_error)
			{
				BYTE type = 0;
				struct GamePortTrigger gpt;

				gameport_io_msg->io_Command = GPD_ASKCTYPE;
				gameport_io_msg->io_Length = 1;
				gameport_io_msg->io_Data = (APTR) &type;

				DoIO ((struct IORequest*)gameport_io_msg);

				if(type == GPCT_ALLOCATED)
				{
					printf("gameport already in use\n");
					CloseDevice((struct IORequest*)gameport_io_msg);
					gameport_error = TRUE;
					FreeJoystick();
					return FALSE;
				}

				type = GPCT_ABSJOYSTICK;

				gameport_io_msg->io_Command = GPD_SETCTYPE;
				gameport_io_msg->io_Length = 1;
				gameport_io_msg->io_Data = &type;

				DoIO ((struct IORequest*)gameport_io_msg);

				if(gameport_io_msg->io_Error)
					printf ("Failed to set controller type\n");

				gpt.gpt_Keys = GPTF_DOWNKEYS | GPTF_UPKEYS;
				gpt.gpt_Timeout = 0;
				gpt.gpt_XDelta = 1;
				gpt.gpt_YDelta = 1;

				gameport_io_msg->io_Command = GPD_SETTRIGGER;
				gameport_io_msg->io_Length = sizeof (gpt);
				gameport_io_msg->io_Data = (APTR) &gpt;

				DoIO ((struct IORequest*)gameport_io_msg);

				if(gameport_io_msg->io_Error)
					printf ("Failed to set controller trigger\n");

				gameport_io_msg->io_Command = GPD_READEVENT;
				gameport_io_msg->io_Length = sizeof (struct InputEvent);
				gameport_io_msg->io_Data = (APTR) &gameport_data;
				gameport_io_msg->io_Flags = 0;

				SendIO ((struct IORequest*)gameport_io_msg);
				return TRUE;
			}
		}
	}
	return FALSE;
}
//-------------------------------------
VOID FreeSound(void)
{
//	if(ahi_audio_ctrl) AHI_FreeAudio(ahi_audio_ctrl);

	if(ahi_soundreq[0])
	{
		if(!CheckIO((struct IORequest*)ahi_soundreq[0]))
		{
			AbortIO((struct IORequest*)ahi_soundreq[0]);
			WaitIO((struct IORequest*)ahi_soundreq[0]);
		}
	}

	if(ahi_soundreq[1])
	{
		if(!CheckIO((struct IORequest*)ahi_soundreq[1]))
		{
			AbortIO((struct IORequest*)ahi_soundreq[1]);
			WaitIO((struct IORequest*)ahi_soundreq[1]);
		}
	}

	if(ahi_request )
	{
		if(AHIBase)
		{
			if(!CheckIO((struct IORequest*)ahi_request))
			{
				AbortIO((struct IORequest*)ahi_request);
				WaitIO((struct IORequest*)ahi_request);
			}
			CloseDevice((struct IORequest*)ahi_request);
		}
		DeleteIORequest((struct IORequest*)ahi_request);
	}

	if(ahi_msg_port) DeleteMsgPort(ahi_msg_port);

	if(ahi_streambuf[0]) FreeVec(ahi_streambuf[0]);
	if(ahi_streambuf[1]) FreeVec(ahi_streambuf[1]);
}
//-------------------------------------
BOOL SetupSound(void)
{
	if(tv_mode== TV_PAL)
	{
		ahi_fps = 50;
		ahi_rupf = 312;
	}	else
	{
		ahi_fps = 60;
		ahi_rupf = 262;
	}

	if(!SoundEnabled) return TRUE;

	if((ahi_msg_port = CreateMsgPort()))
	{
		if((ahi_request = (struct AHIRequest *) CreateIORequest(
           ahi_msg_port, sizeof(struct AHIRequest))))
		{
			ahi_request->ahir_Version = 4;  /* Open at least version 4. */

			if(!OpenDevice(AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *) ahi_request, NULL))
			{
				AHIBase = (struct Library *) ahi_request->ahir_Std.io_Device;

				ahi_streamlen = ahi_streamfreq/ahi_fps;
				ahi_samplepos = 0;

				Pokey_sound_init(FREQ_17_EXACT, ahi_streamfreq, 1);

				if((ahi_streambuf[0] = (STRPTR)AllocVec(ahi_streamlen, MEMF_PUBLIC|MEMF_CLEAR)))
				{
					if((ahi_streambuf[1] = (STRPTR)AllocVec(ahi_streamlen, MEMF_PUBLIC|MEMF_CLEAR)))
					{
						ahi_request->ahir_Std.io_Message.mn_Node.ln_Pri = -50;
						ahi_request->ahir_Std.io_Command  = CMD_WRITE;
						ahi_request->ahir_Std.io_Data     = NULL;
						ahi_request->ahir_Std.io_Length   = ahi_streamlen;
						ahi_request->ahir_Std.io_Offset   = 0;
						ahi_request->ahir_Frequency       = ahi_streamfreq;
						ahi_request->ahir_Type            = AHIST_M8S;
						ahi_request->ahir_Volume          = 0x10000;          // Full volume
						ahi_request->ahir_Position        = 0x8000;           // Centered
						ahi_request->ahir_Link            = NULL;
	
						if((ahi_soundreq[0] = (struct AHIRequest*)AllocVec(sizeof(struct AHIRequest),MEMF_PUBLIC)))
						{
							if((ahi_soundreq[1] = (struct AHIRequest*)AllocVec(sizeof(struct AHIRequest),MEMF_PUBLIC)))
							{
								return TRUE;
							}
							FreeVec(ahi_soundreq[0]);
							ahi_soundreq[0] = NULL;
						}
						FreeVec(ahi_streambuf[1]);
						ahi_streambuf[1] = NULL;
					}
					FreeVec(ahi_streambuf[0]);
					ahi_streambuf[0] = NULL;
				}
				CloseDevice((struct IORequest*)ahi_request);
			}
			DeleteIORequest((struct IORequest*)ahi_request);
			ahi_request = NULL;
		}
		DeleteMsgPort(ahi_msg_port);
		ahi_msg_port = NULL;
	}
	SoundEnabled = FALSE;
}
//-------------------------------------
VOID FreeSoundFile(void)
{
	if(ahi_soundhandle)
	{
		CloseAsync(ahi_soundhandle);
		ahi_soundhandle = NULL;
	}
}
//-------------------------------------
VOID SetupSoundFile(void)
{
	FreeSoundFile();

	if(ahi_soundpath)
	{
		ahi_soundhandle = OpenAsync(ahi_soundpath,MODE_WRITE,8192);
	}
}
//-------------------------------------
VOID FreeTimer(void)
{
	if( timer_request )
	{
		if( !timer_error) CloseDevice((struct IORequest *)timer_request);

		DeleteIORequest(timer_request);
		timer_request = NULL;
	}
	if( timer_msg_port )
	{
		DeleteMsgPort(timer_msg_port);
		timer_msg_port = NULL;
	}
}
//-------------------------------------
BOOL SetupTimer(void)
{
	if((timer_msg_port = CreateMsgPort()))
	{
		timer_request = (struct timerequest*)CreateIORequest( timer_msg_port, sizeof( struct timerequest ));
		if( timer_request )
		{
			timer_error = OpenDevice( TIMERNAME, UNIT_MICROHZ, (struct IORequest*)timer_request, NULL );
			if( !timer_error )
			{
				TimerBase = timer_request->tr_node.io_Device;
				return TRUE;
			}
		}
	}
	return FALSE;
}
//-------------------------------------

//-------------------------------------
ASM LONG Settings_ScreenType_Func(register __a1 ULONG *active)
{
	if(*active)
	{
		set(settings_screenmode_popup,MUIA_Disabled, TRUE);
		set(settings_best_check,MUIA_Disabled, TRUE);
		set(settings_overlay_check,MUIA_Disabled, FALSE);
		set(settings_scalable_check,MUIA_Disabled, FALSE);
	}	else
	{
		set(settings_screenmode_popup,MUIA_Disabled, getbool(settings_best_check));
		set(settings_best_check,MUIA_Disabled, FALSE);
		set(settings_overlay_check,MUIA_Disabled, TRUE);
		set(settings_scalable_check,MUIA_Disabled, TRUE);
	}
	return 0;
}
//-------------------------------------
ASM ULONG Settings_ScreenMode_StartFunc(register __a0 struct Hook *hook, register __a2 Object *popasl, register __a1 struct TagItem *taglist)
{
	static struct TagItem tl[]=
	{
		ASLSM_InitialDisplayID,0,
		ASLSM_InitialDisplayDepth,8,
		ASLSM_MinDepth,8,
		ASLSM_MaxDepth,8,
		TAG_DONE
	};

	LONG	i;

	for(i = 0; taglist[i].ti_Tag != TAG_DONE; i++);

	taglist[i].ti_Tag	= TAG_MORE;
	taglist[i].ti_Data	= (ULONG)tl;

	tl[0].ti_Data = DisplayID;

	return(TRUE);
}
//-------------------------------------
ASM ULONG Settings_ScreenMode_StopFunc(register __a0 struct Hook *hook, register __a2 Object *popasl, register __a1 struct ScreenModeRequester *smreq)
{
	DisplayID = smreq->sm_DisplayID;
	set(settings_screenmode_text, MUIA_Text_Contents,GetDisplayName(DisplayID));

	return(0);
}
//-------------------------------------

//-------------------------------------
VOID FreeMUI(void)
{
	if(app) MUI_DisposeObject(app);
	if(MUIMasterBase) CloseLibrary(MUIMasterBase);
	app = NULL;
	MUIMasterBase = NULL;
}
//-------------------------------------
BOOL SetupMUI(void)
{
	static STRPTR SettingsPages[] = {"General","Paths","Graphics","Sound",NULL};
	static STRPTR ScreenTypeEntries[] = {"Custom","Workbench",NULL};
	static STRPTR AtariTypesEntries[] = {"Atari OS/A","Atari OS/B","Atari 800XL","Atari 130XE","Atari 320XE (RAMBO)","Atari 320XE (COMPY SHOP)","Atari 5200",NULL};

	if((MUIMasterBase = OpenLibrary(MUIMASTER_NAME,MUIMASTER_VMIN)))
	{
		settings_screenmode_starthook.h_Entry = (HOOKFUNC)Settings_ScreenMode_StartFunc;
		settings_screenmode_stophook.h_Entry = (HOOKFUNC)Settings_ScreenMode_StopFunc;

		app = ApplicationObject,
				MUIA_Application_Title      , "Atari800",

#ifdef __MAXON__
				MUIA_Application_Version    , "$VER: Atari800 1.0 ("__DATE2__")",
#else
				MUIA_Application_Version    , "$VER: Atari800 1.0 "__AMIGADATE__,
#endif
				MUIA_Application_Copyright  , "©1999 by Sebastian Bauer",
				MUIA_Application_Author     , "Sebastian Bauer",
				MUIA_Application_Description, "Emulates an Atari 8 bit Computer",
				MUIA_Application_Base       , "Atari800",

				SubWindow, settings_wnd = WindowObject,
						MUIA_Window_Title, "Atari 800 - Settings",
						MUIA_Window_ID, 'SETT',
						MUIA_Window_ScreenTitle, "Atari 800 - ©1999 by Sebastian Bauer",

						WindowContents, VGroup,
								Child, RegisterGroup(SettingsPages),
										MUIA_Register_Frame,TRUE,

										Child, VGroup,
												Child, HGroup,
														Child, MakeLabel("Model"),
														Child, settings_model_cycle = MakeCycle(AtariTypesEntries),
												End,

												Child, ColGroup(3),
														Child, HSpace(0),
														Child, MakeLabel1("Enable RAM between $C000 and $CFFF"),
														Child, settings_ram_check = MakeCheck(TRUE),

														Child, HSpace(0),
														Child, MakeLabel1("Patch OS for faster IO"),
														Child, settings_patch_check = MakeCheck(TRUE),

														Child, HSpace(0),
														Child, MakeLabel1("Hold Option while booting"),
														Child, settings_holdoption_check = MakeCheck(TRUE), 
														End,
												End,

										Child, VGroup,
												Child, ColGroup(2),

														Child, MakeLabel("Atari 800 OS/A"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_osa_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopFile),
																End,
		
														Child, MakeLabel("Atari 800 OS/B"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_osb_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopFile),
																End,
		
														Child, MakeLabel("Atari 800XL"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_xlxe_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopFile),
																End,
		
														Child, MakeLabel("Atari Basic"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_basic_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopFile),
																End,
														End,
												Child, VSpace(2),
												Child, ColGroup(2),

														Child, MakeLabel("Cartriges"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_cartriges_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopDrawer),
																ASLFR_DrawersOnly, TRUE,
																End,

														Child, MakeLabel("Disks"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_disks_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopDrawer),
																ASLFR_DrawersOnly, TRUE,
																End,

														Child, MakeLabel("EXEs"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_exes_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopDrawer),
																ASLFR_DrawersOnly, TRUE,
																End,

														Child, MakeLabel("States"),
														Child, PopaslObject,
																MUIA_Popstring_String, settings_states_string = MakeString("",256),
																MUIA_Popstring_Button, MakeImageButton(MUII_PopDrawer),
																ASLFR_DrawersOnly, TRUE,
																End,
														End,
												End,

										Child, VGroup,
												Child, ColGroup(2),
														Child, MakeLabel("Frames to Skip"),
														Child, settings_skipframe_slider = SliderObject,
																MUIA_Numeric_Max,7,
																End,
														End,

												Child, ColGroup(2),
														Child, MakeLabel("Screentype"),
														Child, settings_screentype_cycle = MakeCycle(ScreenTypeEntries),
		
														Child, MakeLabel("Screenmode"),
														Child, settings_screenmode_popup = PopaslObject,
																MUIA_Popstring_String, settings_screenmode_text = TextObject,
																		TextFrame,
																		MUIA_Text_SetMin, FALSE,
																		End,
																MUIA_Popstring_Button, MakeImageButton(MUII_PopUp),
																MUIA_Popasl_Type, ASL_ScreenModeRequest,
																MUIA_Popasl_StartHook,	(ULONG)&settings_screenmode_starthook,
																MUIA_Popasl_StopHook,	(ULONG)&settings_screenmode_stophook,
																End,
														End,

												Child, ColGroup(3),
														Child, HSpace(0),
														Child, MakeLabel1("Use best screenmode"),
														Child, settings_best_check = MakeCheck(FALSE),

														Child, HSpace(0),
														Child, MakeLabel1("Use Overlay if possible"),
														Child, settings_overlay_check = MakeCheck(FALSE),

														Child, HSpace(0),
														Child, MakeLabel1("Scalable Window"),
														Child, settings_scalable_check = MakeCheck(FALSE),
														End,
												End,

										Child, ColGroup(2),
												Child, MakeLabel1("Enable Sound"),
												Child, settings_sound_check = MakeCheck(TRUE),
												End,

										End,
								Child, HGroup,
										Child, settings_save_button = MakeButton("Save"),
										Child, settings_use_button = MakeButton("Use"),
										Child, settings_cancel_button = MakeButton("Cancel"),
										End,
								End,
						End,
				End;

		if(app)
		{
			settings_screentype_hook.h_Entry = (HOOKFUNC)Settings_ScreenType_Func;

			DoMethod(settings_screentype_cycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, settings_screentype_cycle, 3, MUIM_CallHook,&settings_screentype_hook,MUIV_TriggerValue);
			DoMethod(settings_best_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, settings_screenmode_popup, 3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);

			DoMethod(settings_wnd,MUIM_Notify,MUIA_Window_CloseRequest,TRUE, app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);

			DoMethod(settings_save_button,MUIM_Notify,MUIA_Pressed, FALSE, app,2,MUIM_Application_ReturnID,GUI_SAVE);
			DoMethod(settings_use_button,MUIM_Notify,MUIA_Pressed, FALSE, app,2,MUIM_Application_ReturnID,GUI_USE);
			DoMethod(settings_cancel_button,MUIM_Notify,MUIA_Pressed, FALSE, app,2,MUIM_Application_ReturnID,GUI_CANCEL);

			return TRUE;
		}
	}
	return FALSE;
}
//-------------------------------------
VOID SetGadgets(void)
{
	IMPORT Machine machine;
	IMPORT int hold_option;
	IMPORT int enable_sio_patch;
	IMPORT int enable_c000_ram;

	LONG modelAct=0;

	setcycle(settings_screentype_cycle,1);
	setcycle(settings_screentype_cycle,0);

	switch(machine)
	{
		case	Atari:
					break;

		case	AtariXL:
					modelAct=2;
					break;

		case	AtariXE:
					modelAct=3;
					break;

		case	Atari320XE:
					modelAct=4;
					break;

		case	Atari5200:
					modelAct=6;
					break;
	}

	set(settings_model_cycle, MUIA_Cycle_Active, modelAct);
	set(settings_holdoption_check, MUIA_Selected, hold_option);
	set(settings_patch_check, MUIA_Selected, enable_sio_patch);
	set(settings_ram_check, MUIA_Selected, enable_c000_ram);

	if(DisplayID == INVALID_ID) DisplayID = GetBestID(ATARI_WIDTH-64,ATARI_HEIGHT,8);

	set(settings_sound_check, MUIA_Selected, SoundEnabled);
	set(settings_best_check, MUIA_Selected, UseBestID);
	set(settings_screenmode_text, MUIA_Text_Contents,GetDisplayName(DisplayID));

	setstring(settings_osa_string,atari_osa_filename);
	setstring(settings_osb_string,atari_osb_filename);
	setstring(settings_xlxe_string,atari_xlxe_filename);
	setstring(settings_basic_string,atari_basic_filename);
	setstring(settings_cartriges_string,atari_rom_dir);
	setstring(settings_disks_string,DiskPath);
	setstring(settings_exes_string,atari_exe_dir);
	setstring(settings_states_string,atari_state_dir);
}
//-------------------------------------
VOID SetAtari(void)
{
	IMPORT Machine machine;
	IMPORT int hold_option;
	IMPORT int enable_sio_patch;
	IMPORT int enable_c000_ram;

	switch(xget(settings_model_cycle, MUIA_Cycle_Active))
	{
		case	0:
					machine = Atari;
					break;

		case	1:
					machine = Atari;
					break;

		case	2:
					machine = AtariXL;
					break;

		case	3:
					machine = AtariXE;
					break;

		case	4:
					machine = Atari320XE;
					break;

		case	5:
					machine = Atari320XE;
					break;

		case	6:
					machine = Atari5200;
					break;
	}

	enable_c000_ram = getbool(settings_ram_check);
	hold_option = getbool(settings_holdoption_check);
	enable_sio_patch = getbool(settings_patch_check);

	if( !xget(settings_screentype_cycle,MUIA_Cycle_Active))
	{
		ScreenType = CUSTOMSCREEN;
	}	else ScreenType = WBENCHSCREEN;

	Overlay = getbool(settings_overlay_check);
	Scalable = getbool(settings_scalable_check);
	UseBestID = getbool(settings_best_check);
	SoundEnabled = getbool(settings_sound_check);


	strcpy(atari_osa_filename,getstr(settings_osa_string));
	strcpy(atari_osb_filename,getstr(settings_osb_string));
	strcpy(atari_xlxe_filename,getstr(settings_xlxe_string));
	strcpy(atari_basic_filename,getstr(settings_basic_string));
	strcpy(atari_rom_dir,getstr(settings_cartriges_string));
	strcpy(atari_exe_dir,getstr(settings_exes_string));
	strcpy(atari_state_dir,getstr(settings_states_string));

	if(DiskPath) FreeVec(DiskPath);
	DiskPath = StrCopy(getstr(settings_disks_string));
}
//-------------------------------------
VOID Configure(void)
{
	ULONG sigs=0;
	ULONG retID;
	BOOL ready = FALSE;

	SetGadgets();
	set(settings_wnd, MUIA_Window_Open, TRUE);

	while(ready==FALSE)
	{
		switch(retID = DoMethod(app,MUIM_Application_NewInput,&sigs))
		{
			case	GUI_CANCEL:
						ready = TRUE;
						break;

			case	GUI_USE:
						ready = TRUE;
						break;

			case	GUI_SAVE:
						ready = TRUE;
						break;
		}

		if(!ready)
		{
			sigs = Wait(sigs | SIGBREAKF_CTRL_C );
			if(sigs & SIGBREAKF_CTRL_C)
			{
				ready = TRUE;
				break;
			}
		}
	}

	SetAtari();

	set(settings_wnd, MUIA_Window_Open, FALSE);

	if(retID == GUI_SAVE) RtConfigSave();
}

//-------------------------------------

//-------------------------------------
VOID Project_RecordSound(void)
{
	struct MenuItem *mi = (struct MenuItem*)FindUserData(MenuMain,(APTR)MEN_PROJECT_RECORDSOUND);
	if(mi)
	{
		if(mi->Flags & CHECKED) SetupSoundFile();
		else FreeSoundFile();
	}
}
//-------------------------------------
VOID Project_SelectSoundPath(void)
{
	struct FileRequester *filereq = (struct FileRequester*)AllocAslRequest( ASL_FileRequest, NULL);
	if(filereq)
	{
		if(AslRequestTags(filereq,
							ASLFR_DoSaveMode,TRUE,
							ASLFR_InitialDrawer, "PROGDIR:",
							TAG_DONE))
		{
			if(ahi_soundpath) FreeVec(ahi_soundpath);
			ahi_soundpath = GetFullPath(filereq->fr_Drawer, filereq->fr_File);
		}
	}
}
//-------------------------------------
int HandleMenu(UWORD code)
{
	int keycode = -1;

	while( code != MENUNULL )
	{
		struct MenuItem *mi = ItemAddress( MenuMain, code );
		if( mi )
		{
			LONG udata = (LONG)GTMENUITEM_USERDATA( mi );
			switch(udata)
			{
				case	MEN_PROJECT_ABOUT:
//							DisplayAboutWindow();
							break;

				case	MEN_PROJECT_LOADSTATE:
							if(AslRequestTags(StateFileReq,
											ASLFR_DoSaveMode, FALSE,
											ASLFR_Screen, ScreenMain,
											TAG_DONE))
							{
								BPTR cd = Lock(StateFileReq->fr_Drawer, ACCESS_READ);
								if(cd)
								{
									BPTR odir = CurrentDir(cd);
									ReadAtariState(StateFileReq->fr_File,"rb");
									CurrentDir(odir);
									UnLock(cd);
								}
							}
							break;

				case	MEN_PROJECT_SAVESTATE:
							if(AslRequestTags(StateFileReq,
											ASLFR_DoSaveMode, TRUE,
											ASLFR_Screen, ScreenMain,
											TAG_DONE))
							{
								BPTR cd = Lock(StateFileReq->fr_Drawer, ACCESS_READ);
								if(cd)
								{
									BPTR odir = CurrentDir(cd);
									STRPTR fileName = AddSuffix(StateFileReq->fr_File,".sav");
									if(fileName)
									{
										SaveAtariState(fileName,"wb",TRUE);
										FreeVec(fileName);
									}
									UnLock(cd);
								}
							}
							break;

				case	MEN_PROJECT_LOADBIN:
							if(AslRequestTags(BinFileReq,
											ASLFR_DoSaveMode, FALSE,
											ASLFR_Screen, ScreenMain,
											TAG_DONE))
							{
								BPTR cd = Lock(BinFileReq->fr_Drawer, ACCESS_READ);
								if(cd)
								{
									BPTR odir = CurrentDir(cd);
									BIN_loader(BinFileReq->fr_File);
									CurrentDir(odir);
									UnLock(cd);
								}
							}
							break;

				case	MEN_PROJECT_RECORDSOUND:
							Project_RecordSound();
							break;
				
				case	MEN_PROJECT_SOUNDPATH:
							Project_SelectSoundPath();
							break;

				case	MEN_PROJECT_ICONIFY:
							Iconify();
							return -1;

				case	MEN_PROJECT_HELP:
							break;

				case	MEN_PROJECT_QUIT:
							if(DisplayYesNoWindow())
							{
								keycode = AKEY_EXIT;
							}
							break;

				case	MEN_SYSTEM_BOOT:
							if(InsertDisk (1))
							{
								consol = 7;
								keycode = AKEY_COLDSTART;
							}
							break;

				case	MEN_SYSTEM_ID1:
							if(InsertDisk(1))
							{
							}
							break;

				case	MEN_SYSTEM_ID2:
							if(InsertDisk(2))
							{
							}
							break;

				case	MEN_SYSTEM_ID3:
							if(InsertDisk(3))
							{
							}
							break;

				case	MEN_SYSTEM_ID4:
							if(InsertDisk(4))
							{
							}
							break;

				case	MEN_SYSTEM_ID5:
							if(InsertDisk(5))
							{
							}
							break;

				case	MEN_SYSTEM_ID6:
							if(InsertDisk(6))
							{
							}
							break;

				case	MEN_SYSTEM_ID7:
							if(InsertDisk(7))
							{
							}
							break;

				case	MEN_SYSTEM_ID8:
							if(InsertDisk(8))
							{
							}
							break;

				case	MEN_SYSTEM_ED1:
							SIO_Dismount(1);
							break;

				case	MEN_SYSTEM_ED2:
							SIO_Dismount(2);
							break;

				case	MEN_SYSTEM_ED3:
							SIO_Dismount(3);
							break;

				case	MEN_SYSTEM_ED4:
							SIO_Dismount(4);
							break;

				case	MEN_SYSTEM_ED5:
							SIO_Dismount(5);
							break;

				case	MEN_SYSTEM_ED6:
							SIO_Dismount(6);
							break;

				case	MEN_SYSTEM_ED7:
							SIO_Dismount(7);
							break;

				case	MEN_SYSTEM_ED8:
							SIO_Dismount(8);
							break;

				case	MEN_SYSTEM_IC8K:
							InsertROM(0);
							break;

				case	MEN_SYSTEM_IC16K:
							InsertROM(1);
							break;

				case	MEN_SYSTEM_ICOSS:
							InsertROM(2);
							break;

				case	MEN_SYSTEM_REMOVEC:
						  Remove_ROM();
						  Coldstart();
							break;

				case	MEN_SYSTEM_PILL:
						  EnablePILL();
						  Coldstart();
							break;

				case	MEN_SYSTEM_ATARI800A:
							Initialise_AtariOSA();
							break;

				case	MEN_SYSTEM_ATARI800B:
							Initialise_AtariOSB();
							break;

				case	MEN_SYSTEM_ATARI800XL:
							Initialise_AtariXL();
							break;

				case	MEN_SYSTEM_ATARI130XL:
							Initialise_AtariXE();
							break;

				case	MEN_SYSTEM_UI:
						keycode = AKEY_UI;
						break;

				case	MEN_CONSOLE_RESET:
							keycode = AKEY_WARMSTART;
							consol = 7;
							break;

				case	MEN_CONSOLE_OPTION:
							consol &= 0x03;
							keycode = AKEY_NONE;
							break;

				case	MEN_CONSOLE_SELECT:
							consol &= 0x05;
							keycode = AKEY_NONE;
							break;

				case	MEN_CONSOLE_START:
							consol &= 0x06;
							keycode = AKEY_NONE;
							break;

				case	MEN_CONSOLE_HELP:
							keycode = AKEY_HELP;
							break;

				case	MEN_CONSOLE_BREAK:
							keycode = AKEY_BREAK;
							break;

				case	MEN_CONSOLE_COLDSTART:
							keycode = AKEY_COLDSTART;
							consol = 7;
							break;

				case	MEN_SETTINGS_FRAMERATE:
							{
								if(mi->Flags & CHECKED) ShowFPS = TRUE;
								else ShowFPS = FALSE;
							}
							break;

			}

/*	if (key_buf[0x3c])	// F2
		consol &= 0x03;	// OPTION key ON
	else
		consol |= 0x04;	// OPTION key OFF
	if (key_buf[0x3d])	// F3
		consol &= 0x05;	// SELECT key ON
	else
		consol |= 0x02;	// SELECT key OFF
	if (key_buf[0x3e])	// F4
		consol &= 0x06;	// START key ON
	else
		consol |= 0x01;	// START key OFF
*/

			code = mi->NextSelect;
		}	else code = MENUNULL;
	}


	return keycode;
}

//-------------------------------------

//-------------------------------------
VOID gettimeofday( struct timeval *tv, APTR null )
{
	if( TimerBase )
	{
/*		EClockVal e;
		ULONG ticks = ReadEClock(&e);
		unsigned long long micro = (e.ev_lo % ticks) * 1000000 / ticks;

		tv->tv_secs = e.ev_lo / ticks;
		tv->tv_micro = micro;
*/
		GetSysTime(tv);
	}
}
//-------------------------------------
LONG strcasecmp(char *str1, char *str2)
{
	return stricmp(str1,str2);
}
//-------------------------------------

//-------------------------------------
void UpdateSound(void)
{
	static long send[2] = {NULL,NULL};

	if(SoundEnabled)
	{
		if(send[ahi_current])
		{
			if(!CheckIO(((struct IORequest*)ahi_soundreq[ahi_current])))
			{
				AbortIO(((struct IORequest*)ahi_soundreq[ahi_current]));
			}
			WaitIO(((struct IORequest*)ahi_soundreq[ahi_current]));
		}
	
		Pokey_process(ahi_streambuf[ahi_current] + ahi_samplepos, ahi_streamlen - ahi_samplepos);
	
		ahi_updatecount = 0;
		ahi_samplepos = 0;
	
		*ahi_soundreq[ahi_current] = *ahi_request;
		ahi_soundreq[ahi_current]->ahir_Std.io_Data = ahi_streambuf[ahi_current];
	
		SendIO((struct IORequest*)ahi_soundreq[ahi_current]);
		send[ahi_current] = TRUE;
	
		if(ahi_soundhandle)
		{
			WriteAsync(ahi_soundhandle, ahi_streambuf[ahi_current], ahi_streamlen);
		}
	
		if(ahi_current) ahi_current=0;
		else ahi_current=1;
	}
}
//-------------------------------------

//-------------------------------------
void Atari_ConfigInit(void)
{
	strcpy(atari_osa_filename, "PROGDIR:atariosa.rom");
	strcpy(atari_osb_filename, "PROGDIR:atariosb.rom");
	strcpy(atari_xlxe_filename, "PROGDIR:atarixl.rom");
	strcpy(atari_basic_filename, "PROGDIR:ataribas.rom");
	atari_5200_filename[0] = '\0';
	atari_disk_dirs[0][0] = '\0';
	disk_directories = 0;
	strcpy(atari_rom_dir,"PROGDIR:Cartriges");
	atari_h1_dir[0] = '\0';
	atari_h2_dir[0] = '\0';
	atari_h3_dir[0] = '\0';
	atari_h4_dir[0] = '\0';
	strcpy(atari_exe_dir,"PROGDIR:EXEs");
	strcpy(atari_state_dir,"PROGDIR:States");
}
//-------------------------------------
int Atari_Configure(char* option,char *parameters)
{
	if(!strcmp(option,"AMIGA_GFX_USEBESTMODE")) StrToLong(parameters,&UseBestID);
	else if(!strcmp(option,"AMIGA_GFX_USEOVERLAY")) StrToLong(parameters,&Overlay);
	else if(!strcmp(option,"AMIGA_GFX_SCALABLE")) StrToLong(parameters, &Scalable);
	else if(!strcmp(option,"AMIGA_GFX_DISPLAYID")) sscanf(parameters,"0x%lx",&DisplayID);
	else if(!strcmp(option,"AMIGA_GFX_SCREENTYPE"))
	{
		if(!strcmp(parameters,"CUSTOM")) ScreenType = CUSTOMSCREEN;
		else ScreenType = WBENCHSCREEN;
	}
	else if(!strcmp(option,"AMIGA_PATHS_DISKS"))
	{
		DiskPath = StrCopy(parameters);
		DiskPathSet = TRUE;
	}
	else if(!strcmp(option,"AMIGA_SOUND"))
	{
		if(!strcmp(parameters,"AHI")) SoundEnabled=TRUE;
		else SoundEnabled=FALSE;
	}	else return FALSE;
	return TRUE;
}
//-------------------------------------
void Atari_ConfigSave( FILE *fp)
{
	fprintf(fp,"AMIGA_PATHS_DISKS=%s\n",DiskPath);

	fputs("AMIGA_GFX_SCREENTYPE=",fp);
	if(ScreenType == CUSTOMSCREEN) fputs("CUSTOM\n",fp);
	else fputs("WORKBENCH\n",fp);

	fprintf(fp,"AMIGA_GFX_DISPLAYID=0x%lx\n",DisplayID);
	fprintf(fp,"AMIGA_GFX_USEBESTMODE=%ld\n",UseBestID);
	fprintf(fp,"AMIGA_GFX_USEOVERLAY=%ld\n",Overlay);
	fprintf(fp,"AMIGA_GFX_SCALABLE=%ld\n",Scalable);

	fputs("AMIGA_SOUND=",fp);
	if(SoundEnabled) fputs("AHI\n",fp);
	else fputs("NO\n",fp);
}
//-------------------------------------
void Atari_Initialise (int *argc, unsigned char **argv)
{
	Controller = controller_Joystick;
	PaddlePos = 228;

	if(!DiskPathSet)
	{
		DiskPathSet=TRUE;
		DiskPath=StrCopy("PROGDIR:Disks");
	}

	if(OpenLibraries())
	{
		if(SetupMUI())
		{
			Configure();

			if((DiskFileReq = AllocAslRequestTags( ASL_FileRequest,
												ASLFR_DoPatterns, TRUE,
												ASLFR_InitialPattern,"#?.(atr|xfd)",
												ASLFR_InitialDrawer,DiskPath?DiskPath:"",
												TAG_DONE )))
			{
				if((StateFileReq = AllocAslRequestTags( ASL_FileRequest,
											ASLFR_DoPatterns, TRUE,
											ASLFR_InitialPattern,"#?.sav",
											ASLFR_InitialDrawer,atari_state_dir,
											TAG_DONE )))
				{
					if((BinFileReq = AllocAslRequestTags( ASL_FileRequest,
											ASLFR_DoPatterns, TRUE,
											ASLFR_InitialPattern,"#?.(bin|exe)",
											ASLFR_InitialDrawer,atari_exe_dir,
											TAG_DONE )))
					{
						SetupJoystick();
						SetupSound();

						if(SetupTimer())
						{
							SetupDisplay();

							SetMenuStrip (WindowMain, MenuMain);

							trig0 = 1;
							stick0 = 15;
							consol = 7;
						}
					}
				}
			}
		}
	}
}
//-------------------------------------
int Atari_Exit (int run_monitor)
{
	if (run_monitor)
	{
		if (monitor ())
		{
			return TRUE;
		}
	}

	if(DiskPath) FreeVec(DiskPath);

	FreeSoundFile();
	FreeJoystick();
	FreeSound();
	FreeDisplay();

	if (DiskFileReq) FreeAslRequest(DiskFileReq);
	if (StateFileReq) FreeAslRequest(StateFileReq);
	if (BinFileReq) FreeAslRequest(BinFileReq);

	FreeMUI();
	CloseLibraries();
	exit(0);
}
//-------------------------------------
void Atari_DisplayScreen(UBYTE *screen)
{
	static char fpsbuf[32];

	if(ShowFPS)
	{
		IMPORT double fps;
		double fpsv;
		double fpsn = modf(fps, &fpsv);

		sprintf(fpsbuf,"%ld.%ld/%ld (%ld%%)",(LONG)fpsv,(LONG)(fpsn*10),ahi_fps,(LONG)(100*fps/ahi_fps));
	}

//	struct timeval tv;
//	struct timeval tv2;
//	gettimeofday(&tv,NULL);

	if( ScreenType == WBENCHSCREEN)
	{
		if(VLHandle)
		{
			if(VLAttached)
			{
				if(LockVLayer(VLHandle))
				{
					ScreenData215bit(screen, (UWORD*)GetVLayerAttr(VLHandle, VOA_BaseAddress),colortable15,ATARI_WIDTH,ATARI_HEIGHT);

/*				ULONG *destdata = (ULONG*)GetVLayerAttr(VLHandle, VOA_BaseAddress);
				UBYTE *srcdata = screen;
	
				destdata = 
	
				for(y=0;y<ATARI_HEIGHT;y++)
				{
					for(x=0; x<ATARI_WIDTH/2; x++)
					{
						UWORD data1 = colortable15[*srcdata++];
						UWORD data2 = colortable15[*srcdata++];
	
						*destdata++ = data1<<16|data2;
					}
				}*/
	
					UnLockVLayer(VLHandle);
				}
			}
		}	else
		{
//			LONG x,y;
			LONG offx = WindowMain->BorderLeft;
			LONG offy = WindowMain->BorderTop;

			ScreenData28bit(screen, tempscreendata,colortable8,ATARI_WIDTH,ATARI_HEIGHT);

/*			UBYTE *srcdata = screen;
			ULONG *destdata = (ULONG*)tempscreendata;

			for(y=offy;y<ATARI_HEIGHT+offy;y++)
			{
				for(x=0;x<ATARI_WIDTH/4;x++)
				{
					UBYTE data1 = colortable8[*srcdata++];
					UBYTE data2 = colortable8[*srcdata++];
					UBYTE data3 = colortable8[*srcdata++];
					UBYTE data4 = colortable8[*srcdata++];

					*destdata++ = (data1<<24)|(data2<<16)|(data3<<8)|data4;
				}
			}*/

			if(Scalable && (InnerWidth(WindowMain)!=ATARI_WIDTH || InnerHeight(WindowMain) != ATARI_HEIGHT))
			{
				ScalePixelArray( tempscreendata, ATARI_WIDTH, ATARI_HEIGHT, ATARI_WIDTH, WindowMain->RPort, offx, offy,
											InnerWidth(WindowMain), InnerHeight(WindowMain), RECTFMT_LUT8);
			}	else
			{
				struct RastPort temprp;
				InitRastPort(&temprp);
				WritePixelArray8( WindowMain->RPort,offx,offy,offx+ATARI_WIDTH-1,offy+ATARI_HEIGHT-1, tempscreendata, &temprp);
			}

			if(ShowFPS)
			{
				LONG len;
	
				SetABPenDrMd(WindowMain->RPort,colortable8[15],colortable8[0],JAM2);
				len = TextLength(WindowMain->RPort,fpsbuf,strlen(fpsbuf));
				Move(WindowMain->RPort, offx + WindowMain->Width - 20 - len, offy + 4 + WindowMain->RPort->TxBaseline);
				Text(WindowMain->RPort, fpsbuf,strlen(fpsbuf));
			}
		}
	}	else
	{
		struct RastPort temprp;
		LONG x = -32;
		LONG y = 0;

		InitRastPort(&temprp);
		WritePixelArray8( WindowMain->RPort,x,y,x+ATARI_WIDTH-1,y+ATARI_HEIGHT-1, screen, &temprp);

		if(ShowFPS)
		{
			LONG len;

			SetABPenDrMd(WindowMain->RPort,15,0,JAM2);
			len = TextLength(WindowMain->RPort,fpsbuf,strlen(fpsbuf));
			Move(WindowMain->RPort, WindowMain->Width - 20 - len, 4 + WindowMain->RPort->TxBaseline);
			Text(WindowMain->RPort, fpsbuf,strlen(fpsbuf));
		}
	}

//	gettimeofday(&tv2,NULL);
//	SubTime(&tv2,&tv);

//	printf("secs: %ld  mics: %ld\n",tv2.tv_secs,tv2.tv_micro);

	UpdateSound();
}

//-------------------------------------
int Atari_Keyboard (void)
{
	int keycode=-1;

	struct IntuiMessage *imsg;

	while (( imsg = (struct IntuiMessage*) GetMsg (WindowMain->UserPort)))
	{
		ULONG cl = imsg->Class;
		UWORD code = imsg->Code;
		UWORD qual = imsg->Qualifier;
		APTR iaddress = imsg->IAddress;
		WORD mx = imsg->MouseX;
		WORD my = imsg->MouseY;

		if(cl == IDCMP_MENUVERIFY)
		{
			if(VLAttached)
			{
				DetachVLayer(VLHandle);
				VLAttached = FALSE;
			}

			ReplyMsg((struct Message*)imsg);
			continue;
		}

		ReplyMsg((struct Message*)imsg);

		switch( cl )
		{
			case	IDCMP_VANILLAKEY:
						keycode = code;
	
						switch (keycode)
						{
							case	0x01 :
										keycode = AKEY_CTRL_a;
										break;
							case	0x02 :
										keycode = AKEY_CTRL_b;
										break;
							case	0x03 :
										keycode = AKEY_CTRL_c;
										break;
							case	0x04 :
										keycode = AKEY_CTRL_d;
										break;
							case	0x05 :
										keycode = AKEY_CTRL_E;
										break;
							case	0x06 :
										keycode = AKEY_CTRL_F;
										break;
							case	0x07 :
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
							case	0x0a :
										keycode = AKEY_CTRL_J;
										break;
							case	0x0b :
										keycode = AKEY_CTRL_K;
										break;
							case	0x0c :
										keycode = AKEY_CTRL_L;
										break;

		/*
							case 0x0d :
								keycode = AKEY_CTRL_M;
								break;
		*/
							case	0x0e :
										keycode = AKEY_CTRL_N;
										break;
							case	0x0f :
										keycode = AKEY_CTRL_O;
										break;
							case	0x10 :
										keycode = AKEY_CTRL_P;
										break;
							case	0x11 :
										keycode = AKEY_CTRL_Q;
										break;
							case	0x12 :
										keycode = AKEY_CTRL_R;
										break;
							case	0x13 :
										keycode = AKEY_CTRL_S;
										break;
							case	0x14 :
										keycode = AKEY_CTRL_T;
										break;
							case	0x15 :
										keycode = AKEY_CTRL_U;
										break;
							case	0x16 :
										keycode = AKEY_CTRL_V;
										break;
							case	0x17 :
										keycode = AKEY_CTRL_W;
										break;
							case	0x18 :
										keycode = AKEY_CTRL_X;
										break;
							case	0x19 :
										keycode = AKEY_CTRL_Y;
										break;
							case	0x1a :
										keycode = AKEY_CTRL_Z;
										break;
							case	8 :
										keycode = AKEY_BACKSPACE;
										break;
							case	13 :
										keycode = AKEY_RETURN;
										break;
							case	0x1b :
										keycode = AKEY_ESCAPE;
										break;
							case	'0' :
										keycode = AKEY_0;
										break;
							case	'1' :
										keycode = AKEY_1;
										break;
							case	'2':
										keycode = AKEY_2;
										break;
							case	'3':
										keycode = AKEY_3;
										break;
							case	'4':
										keycode = AKEY_4;
										break;
							case	'5':
										keycode = AKEY_5;
										break;
							case	'6':
										keycode = AKEY_6;
										break;
							case	'7':
										keycode = AKEY_7;
										break;
							case	'8':
										keycode = AKEY_8;
										break;
							case	'9':
										keycode = AKEY_9;
										break;
							case	'A': case	'a':
										keycode = AKEY_a;
										break;
							case 'B' : case 'b' :
								keycode = AKEY_b;
								break;
							case 'C' : case 'c' :
								keycode = AKEY_c;
								break;
							case 'D' : case 'd' :
								keycode = AKEY_d;
								break;
							case 'E' : case 'e' :
								keycode = AKEY_e;
								break;
							case 'F' : case 'f' :
								keycode = AKEY_f;
								break;
							case 'G' : case 'g' :
								keycode = AKEY_g;
								break;
							case 'H' : case 'h' :
								keycode = AKEY_h;
								break;
							case 'I' : case 'i' :
								keycode = AKEY_i;
								break;
							case 'J' : case 'j' :
								keycode = AKEY_j;
								break;
							case 'K' : case 'k' :
								keycode = AKEY_k;
								break;
							case 'L' : case 'l' :
								keycode = AKEY_l;
								break;
							case 'M' : case 'm' :
								keycode = AKEY_m;
								break;
							case 'N' : case 'n' :
								keycode = AKEY_n;
								break;
							case 'O' : case 'o' :
								keycode = AKEY_o;
								break;
							case 'P' : case 'p' :
								keycode = AKEY_p;
								break;
							case 'Q' : case 'q' :
								keycode = AKEY_q;
								break;
							case 'R' : case 'r' :
								keycode = AKEY_r;
								break;
							case 'S' : case 's' :
								keycode = AKEY_s;
								break;
							case 'T' : case 't' :
								keycode = AKEY_t;
								break;
							case 'U' : case 'u' :
								keycode = AKEY_u;
								break;
							case 'V' : case 'v' :
								keycode = AKEY_v;
								break;
							case 'W' : case 'w' :
								keycode = AKEY_w;
								break;
							case 'X' : case 'x' :
								keycode = AKEY_x;
								break;
							case 'Y' : case 'y' :
								keycode = AKEY_y;
								break;
							case 'Z' : case 'z' :
								keycode = AKEY_z;
								break;
							case ' ' :
								keycode = AKEY_SPACE;
								break;
							case '\t' :
								keycode = AKEY_TAB;
								break;
							case '!' :
								keycode = AKEY_EXCLAMATION;
								break;
							case '"' :
								keycode = AKEY_DBLQUOTE;
								break;
							case '#' :
								keycode = AKEY_HASH;
								break;
							case '$' :
								keycode = AKEY_DOLLAR;
								break;
							case '%' :
								keycode = AKEY_PERCENT;
								break;
							case '&' :
								keycode = AKEY_AMPERSAND;
								break;
							case '\'' :
								keycode = AKEY_QUOTE;
								break;
							case '@' :
								keycode = AKEY_AT;
								break;
							case '(' :
								keycode = AKEY_PARENLEFT;
								break;
							case ')' :
								keycode = AKEY_PARENRIGHT;
								break;
							case '<' :
								keycode = AKEY_LESS;
								break;
							case '>' :
								keycode = AKEY_GREATER;
								break;
							case '=' :
								keycode = AKEY_EQUAL;
								break;
							case '?' :
								keycode = AKEY_QUESTION;
								break;
							case '-' :
								keycode = AKEY_MINUS;
								break;
							case '+' :
								keycode = AKEY_PLUS;
								break;
							case '*' :
								keycode = AKEY_ASTERISK;
								break;
							case '/' :
								keycode = AKEY_SLASH;
								break;
							case ':' :
								keycode = AKEY_COLON;
								break;
							case ';' :
								keycode = AKEY_SEMICOLON;
								break;
							case ',' :
								keycode = AKEY_COMMA;
								break;
							case '.' :
								keycode = AKEY_FULLSTOP;
								break;
							case '_' :
								keycode = AKEY_UNDERSCORE;
								break;
							case '[' :
								keycode = AKEY_BRACKETLEFT;
								break;
							case ']' :
								keycode = AKEY_BRACKETRIGHT;
								break;
							case '^' :
								keycode = AKEY_CIRCUMFLEX;
								break;

							case	'\\' :
										keycode = AKEY_BACKSLASH;
										break;

							case	'|' :
										keycode = AKEY_BAR;
										break;

							default :
										keycode = AKEY_NONE;
										break;
						}
						break;

			case	IDCMP_RAWKEY:
						printf("%lx  %lx\n",code,qual);
						switch (code)
						{
							case	0x51:					/* F2 */
										consol &= 0x03;
										keycode = AKEY_NONE;
										break;
							case	0x52:					/* F3 */
										consol &= 0x05;
										keycode = AKEY_NONE;
										break;
							case	0x53 :					/* F4 */
										consol &= 0x06;
										keycode = AKEY_NONE;
										break;

							case	0x54:					/* F5 */
										if(qual & IEQUALIFIER_SHIFT) keycode = AKEY_COLDSTART;
										else keycode = AKEY_WARMSTART;
										consol = 7;
										break;

							case	0x55:						/* F6 */
										keycode = AKEY_PIL;
										break;
							case	0x56 :
										keycode = AKEY_BREAK;
										break;
							case	0x59 :
										keycode = AKEY_NONE;
										break;
							case	0x5f:					/* HELP */
										keycode = AKEY_HELP;
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

			case	IDCMP_MOUSEBUTTONS :
						if (Controller == controller_Paddle)
						{
							switch (code)
							{
								case	SELECTDOWN :
											stick0 = 251;
											break;

								case	SELECTUP :
											stick0 = 255;
											break;

								default:
											break;
							}
						}
						break;

			case	IDCMP_MOUSEMOVE :
						if (Controller == controller_Paddle)
						{
							if (mx > 57)
							{
								if (mx < 287)
								{
									PaddlePos = 228 - (mx - 58);
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

			case	IDCMP_CLOSEWINDOW:
						if(DisplayYesNoWindow())
						{
							keycode = AKEY_EXIT;
						}
						break;

			case	IDCMP_MENUPICK:
						{
							keycode = HandleMenu(code);
							if(!VLAttached && VLHandle)
							{
								if(!AttachVLayerTags( VLHandle, WindowMain, TAG_DONE)) VLAttached = TRUE;
							}
						}
						break;

			default:
						break;
		}
	}

	return keycode;
}

//-------------------------------------
int Atari_Joystick(void)
{
	WORD x;
	WORD y;
	UWORD code;

	int stick = 0;

	if(GetMsg (gameport_msg_port))
	{
		x = gameport_data.ie_X;
		y = gameport_data.ie_Y;
		code = gameport_data.ie_Code;

		switch(x)
		{
			case	-1:
						switch (y)
						{
							case	-1 :
										stick = 10;
										break;

							case	0:
										stick = 11;
										break;

							case	1:
										stick = 9;
										break;

							default :
										break;
						}
						break;

			case	0:
						switch (y)
						{
							case	-1:
										stick = 14;
										break;

							case	0:
										stick = 15;
										break;

							case	1:
										stick = 13;
										break;

							default :
										break;
						}
						break;

			case	1:
						switch (y)
						{
							case	-1:
										stick = 6;
										break;

							case	0:
										stick = 7;
										break;

							case	1:
										stick = 5;
										break;

							default:
										break;
						}
						break;

			default :
						break;
		}

		if(code == IECODE_LBUTTON)
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
		SendIO ((struct IORequest*)gameport_io_msg);
	}
	
	if (stick == 0)
	{
		stick = old_stick_0;
	}

	stick0 = stick & 0x0f;
	trig0 = (stick & 0x10) >> 4;

/*	printf ("stick0 = %d   trig0 = %d\n", stick0, trig0);
	return stick;
*/
	return stick;
}

//-------------------------------------
int Atari_PORT (int num)

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

//-------------------------------------
int Atari_TRIG (int num)

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

{
	if (num == 0)
	{
		Atari_Joystick ();
		return trig0;
	}
	else
		return 1;
}

//-------------------------------------
int Atari_POT (int num)

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


{
	return PaddlePos;
}

//-------------------------------------
int Atari_CONSOL (void)

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

{
	return consol;
}

//-------------------------------------
void Atari_AUDC(int channel, int byte)
{
	channel--;
	Update_pokey_sound(0xd201 + channel + channel, byte, 0, 4);
}
//-------------------------------------
void Atari_AUDF(int channel, int byte)
{
	channel--;
	Update_pokey_sound(0xd200 + channel + channel, byte, 0, 4);
}
//-------------------------------------
void Atari_AUDCTL(int byte)
{
	Update_pokey_sound(0xd208, byte, 0, 4);
}
//-------------------------------------
void pokey_update(void)
{
	LONG newpos;

	if(!SoundEnabled) return;
	if(ahi_updatecount >= ahi_rupf) return;

	newpos = ahi_streamlen * (ahi_updatecount) / ahi_rupf;
	if(newpos > ahi_streamlen) newpos = ahi_streamlen;

	Pokey_process(ahi_streambuf[ahi_current] + ahi_samplepos, newpos - ahi_samplepos);
	ahi_samplepos = newpos;
	ahi_updatecount++;
}
//-------------------------------------

//-------------------------------------
STATIC VOID DisplayControllerWindow(int Port)

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
{
}
/*	struct Gadget *glist;
	struct Gadget *g = CreateContext( &glist );
	if( g )
	{
		struct NewGadget ng;
		struct TextFont *font = dinfo->dri_Font;
		UWORD fontY = font->tf_YSize;
		UWORD offX = scr->WBorLeft;
		UWORD offY = scr->WBorTop + fontY + 1;

		struct RastPort *rp = &scr->RastPort;

					UWORD modeLeft = TextLength( rp, setupModeText, strlen( setupModeText ))+2;
					UWORD modeWidth = CalcMinLength( rp, setupModeLabels ) + 20;
					UWORD wndWidth = 2*offX + modeLeft + modeWidth + 8;
					UWORD butWidth = (wndWidth - 2*offX - 4)/2-1;
					UWORD wndHeight = offY + 2*(fontY+6)+3+4;

					ng.ng_VisualInfo = vis;
					ng.ng_TextAttr = scr->Font;
					ng.ng_LeftEdge = offX + modeLeft + 2;
					ng.ng_TopEdge = offY + 2;
					ng.ng_Width = modeWidth + 4;
					ng.ng_Height = fontY + 6;
					ng.ng_GadgetID = 1;
					ng.ng_UserData = NULL;
					ng.ng_Flags = PLACETEXT_LEFT;
					ng.ng_GadgetText = setupModeText;

					g = CreateGadget( CYCLE_KIND, g, &ng,
													GTCY_Labels, setupModeLabels,
													GTCY_Active, 1,
													GT_Underscore, '_',
													TAG_DONE );

}
*/
/*static void DisplayControllerWindow (int Port)

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
// Re-insert for v.0.2.2
	NewWindow.Height = 155;

	NewWindow.Height = 92; // 177
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

// Re-insert for v0.2.2
//	Rule (0, 16, 288, WindowController);

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

// Re-insert for v0.2.2
//	ShowText (36, 1, WindowController, "Via:");

	Gadget3 = MakeGadget (38, 3, 16, 14, 0, WindowController, "Joystick", gadget_Mutual);
	Gadget4 = MakeGadget (38, 5, 16, 14, 0, WindowController, "Mouse", gadget_Mutual);

	ShowText (1, 8, WindowController, "Amiga port:");

	Gadget5 = MakeGadget (3, 10, 16, 14, 0, WindowController, "0", gadget_Mutual);
	Gadget6 = MakeGadget (3, 12, 16, 14, 0, WindowController, "1", gadget_Mutual);
	Gadget7 = MakeGadget (3, 14, 16, 14, 0, WindowController, "None", gadget_Mutual);

	Gadget8 = MakeGadget (39, 18, 64, 14, 0, WindowController, "Ok", gadget_Button);
	Gadget9 = MakeGadget (55, 18, 64, 14, 0, WindowController, "Cancel", gadget_Button);


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

			ReplyMsg ((struct Message*)IntuiMessage);

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
// Re-insert for v0.2.2
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
}*/

//-------------------------------------
LONG DisplayYesNoWindow(void)
{
	struct EasyStruct easy;
	easy.es_StructSize = sizeof(struct EasyStruct);
	easy.es_Flags = 0;
	easy.es_Title = "Warning";
	easy.es_TextFormat = "Are you sure?";
	easy.es_GadgetFormat = "OK|Cancel";

	return EasyRequestArgs( WindowMain, &easy, NULL, NULL );
}


//-------------------------------------

//-------------------------------------
LONG InsertDisk( LONG Drive )
{
	char Filename[256];
	int Success = FALSE;

	if( AslRequestTags( DiskFileReq,
					ASLFR_Screen, ScreenMain,
					TAG_DONE))
	{
		SIO_Dismount( Drive );

		strcpy( Filename, DiskFileReq->rf_Dir );
		if( AddPart( Filename, DiskFileReq->rf_File,255) != DOSFALSE )
		{
//			printf("File selected: %s  ",Filename);
			if(SIO_Mount (Drive, Filename))
			{
//				printf("OK\n");
				Success = TRUE;
			}//	else printf("Error!\n");
		}
	}

	return Success;
}

//-------------------------------------
LONG InsertROM(LONG CartType)
{
	struct FileRequester *FileRequester = NULL;
	char Filename[256];
	int Success = FALSE;

	if ((FileRequester = (struct FileRequester*) AllocAslRequestTags (ASL_FileRequest, TAG_DONE)))
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

			if (FileRequester->rf_Dir)
			{
				sprintf (Filename, "%s/%s", FileRequester->rf_Dir, FileRequester->rf_File);
			}
			else
			{
				sprintf (Filename, "%s", FileRequester->rf_File);
			}

			if (CartType == 0)
			{
				Remove_ROM ();
				if (Insert_8K_ROM(Filename))
				{
					Coldstart ();
				}
			}
			else if (CartType == 1)
			{
				Remove_ROM ();
				if (Insert_16K_ROM(Filename))
				{
					Coldstart ();
				}
			}
			else if (CartType == 2)
			{
				Remove_ROM ();
				if (Insert_OSS_ROM(Filename))
				{
					Coldstart ();
				}
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

//-------------------------------------

//-------------------------------------
VOID FreeDisplay(void)
{
	if( MenuMain )
	{
		if( WindowMain ) ClearMenuStrip( WindowMain );
		FreeMenus( MenuMain );
		MenuMain = NULL;
	}

	if( VLHandle)
	{
		DetachVLayer(VLHandle);
		VLAttached = FALSE;
		DeleteVLayerHandle(VLHandle);
		VLHandle = NULL;
	}

	if( WindowMain )
	{
		CloseWindow( WindowMain );
		WindowMain = NULL;
	}

	if( VisualInfoMain )
	{
		FreeVisualInfo( VisualInfoMain );
		VisualInfoMain = NULL;
	}

	if(colortable15)
	{
		FreeVec(colortable15);
		colortable15 = NULL;
	}

	if(colortable8)
	{
		LONG i;
		for(i=0;i<256;i++) ReleasePen(ScreenMain->ViewPort.ColorMap,colortable8[i]);
		FreeVec(colortable8);
		colortable8 = NULL;
	}

	if(tempscreendata)
	{
		FreeVec(tempscreendata);
		tempscreendata=NULL;
	}

	if( ScreenMain )
	{
		if(ScreenType == WBENCHSCREEN)
		{
			UnlockPubScreen(NULL,ScreenMain);
		}	else CloseScreen( ScreenMain );
		ScreenMain = NULL;
	}
}
//-------------------------------------
VOID SetupDisplay(void)
{
	UWORD ScreenWidth, ScreenHeight, ScrDepth;
	int i;

	STATIC WORD ScreenPens[13] =
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

	if( ScreenType == CUSTOMSCREEN)
	{
		ULONG ScreenDisplayID;

		ScreenType = CUSTOMSCREEN;

		ScreenWidth = ATARI_WIDTH - 64;
		ScreenHeight = ATARI_HEIGHT;

		ScrDepth = 8;

		ScreenDisplayID = DisplayID;
		if(UseBestID) ScreenDisplayID = GetBestID(ScreenWidth,ScreenHeight,ScrDepth);

		ScreenMain = OpenScreenTags( NULL,
									SA_Left, 0,
									SA_Top, 0,
									SA_Width, ScreenWidth,
									SA_Height, ScreenHeight,
									SA_Depth, ScrDepth,
									SA_Pens, ScreenPens,
									SA_Quiet, TRUE,
									SA_Type, CUSTOMSCREEN,
									SA_AutoScroll, TRUE,
									SA_LikeWorkbench, TRUE,
									SA_DisplayID,ScreenDisplayID,
									TAG_DONE);

		if( ScreenMain )
		{
			for (i=0;i<256;i++)
			{
				int rgb = colortable[i];
				int red,green,blue;
			
				red = (rgb & 0x00ff0000) >> 20;
				green = (rgb & 0x0000ff00) >> 12;
				blue = (rgb & 0x000000ff) >> 4;
			
				SetRGB4 (&ScreenMain->ViewPort, i, red, green, blue);
			}
		}
	}	else
	{
		ScreenWidth = ATARI_WIDTH;
		ScreenHeight = ATARI_HEIGHT;

		if((ScreenMain = LockPubScreen(NULL)))
		{
			if(Overlay)
			{
				static struct TagItem tl[] =
				{
					VOA_SrcType,SRCFMT_RGB15PC,
					VOA_SrcWidth,ATARI_WIDTH,
					VOA_SrcHeight,ATARI_HEIGHT,
					TAG_DONE
				};

				if((VLHandle = CreateVLayerHandleTagList( ScreenMain, tl)))
				{
					if((colortable15 = (UWORD*)AllocVec(256*sizeof(UWORD),0)))
					{
						LONG i;
						for(i=0;i<256;i++)
						{
							ULONG rgb = colortable[i];
							ULONG red = (rgb & 0x00ff0000) >> 16;
							ULONG green = (rgb & 0x0000ff00) >> 8;
							ULONG blue = (rgb & 0x000000ff);

							UWORD data = ((red>>3)<<10)|((green>>3)<<5)|(blue>>3);
							data = (data>>8)|(data<<8);

							colortable15[i] = data;
						}
					}
				}
			}	else
			{
				if((colortable8 = (UBYTE*)AllocVec(256*sizeof(UBYTE),0)))
				{
					LONG i;
					for(i=0;i<256;i++)
					{
						ULONG rgb = colortable[i];
						ULONG red = (rgb & 0x00ff0000) >> 16;
						ULONG green = (rgb & 0x0000ff00) >> 8;
						ULONG blue = (rgb & 0x000000ff);

						red |= (red<<24)|(red<<16)|(red<<8);
						green |= (green<<24)|(green<<16)|(green<<8);
						blue |= (blue<<24)|(blue<<16)|(blue<<8);

						colortable8[i] = ObtainBestPenA(ScreenMain->ViewPort.ColorMap,red,green,blue,NULL);
					}
					if((tempscreendata = (UBYTE*)AllocVec(ATARI_WIDTH*(ATARI_HEIGHT+16),0)))
					{
					}
				}
			}
		}
	}

	if( ScreenMain )
	{
		VisualInfoMain = GetVisualInfoA( ScreenMain, NULL );
		if( VisualInfoMain )
		{
			MenuMain = CreateMenus( MenuEntries,
											GTMN_NewLookMenus, TRUE,
											TAG_DONE);

			if( MenuMain )
			{
				LayoutMenus( MenuMain, VisualInfoMain, GTMN_NewLookMenus, TRUE, TAG_DONE);
			}
		}
	}

	WindowMain = OpenWindowTags( NULL,
				WA_Activate, TRUE,
				WA_NewLookMenus, TRUE,
				WA_MenuHelp, TRUE,
				WA_InnerWidth, ScreenWidth,
				WA_InnerHeight, ScreenHeight,
				WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW |
									IDCMP_RAWKEY | IDCMP_VANILLAKEY | (VLHandle?IDCMP_MENUVERIFY:0),
				WA_ReportMouse, TRUE,
				WA_CustomScreen, ScreenMain,
				ScreenType==WBENCHSCREEN?TAG_IGNORE:WA_Borderless, TRUE,
				ScreenType==WBENCHSCREEN?WA_CloseGadget:TAG_IGNORE, TRUE,
				ScreenType==WBENCHSCREEN?WA_DragBar:TAG_IGNORE, TRUE,
				ScreenType==WBENCHSCREEN?WA_DepthGadget:TAG_IGNORE,TRUE,
				ScreenType==WBENCHSCREEN?WA_Title:TAG_IGNORE, "Atari 800",
				ScreenType==WBENCHSCREEN?WA_ScreenTitle:TAG_IGNORE, ATARI_TITLE,
				WA_SizeGadget, Scalable,
				Scalable?WA_SizeBBottom:TAG_IGNORE, TRUE,
				Scalable?WA_MaxWidth:TAG_IGNORE,-1,
				Scalable?WA_MaxHeight:TAG_IGNORE,-1,
				TAG_DONE);

	if (!WindowMain)
	{
		printf ("Failed to create window\n");
		Atari_Exit (0);
	}

	if(VLHandle)
	{
		if(AttachVLayerTags( VLHandle, WindowMain, TAG_DONE))
		{
			Atari_Exit(20);
		}
		VLAttached = TRUE;
	}
}

//-------------------------------------
VOID Iconify(void)
{
	struct Window *iconifyWnd;

	FreeDisplay();

	iconifyWnd = OpenWindowTags( NULL,
									WA_InnerHeight,-2,
									WA_InnerWidth, 130,
									WA_CloseGadget, TRUE,
									WA_DragBar, TRUE,
									WA_DepthGadget, TRUE,
									WA_IDCMP, IDCMP_CLOSEWINDOW,
									WA_Title, "Atari800 Emulator",
									TAG_DONE );

	if( iconifyWnd )
	{
		WaitPort( iconifyWnd->UserPort );
		CloseWindow( iconifyWnd );
	}

	SetupDisplay();
	SetMenuStrip (WindowMain, MenuMain );//menu_Project);
}

//-------------------------------------
