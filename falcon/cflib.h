#ifndef _cflib_h_
#define _cflib_h_

#include <stdio.h>
#include <stdlib.h>
#include <osbind.h>
#include <mintbind.h>

#include <gemfast.h>
#include <aesbind.h>
#include <vdibind.h>


/*******************************************************************************
 * 
 * Einige grundlegende Typen
 *
 *******************************************************************************/

typedef int		bool;
#define TRUE	(bool)1
#define FALSE	(bool)0

typedef unsigned char	byte;
typedef unsigned char	uchar;
#ifdef __PUREC__
typedef unsigned int		ushort;
#else
typedef unsigned short	ushort;
#endif
typedef unsigned int		uint;
typedef unsigned long	ulong;

#ifndef EOS
#define EOS		'\0'
#endif

/*******************************************************************************
 * 
 * Globale Variablen						(globals.c)
 * 
 * Sie werden von init_app gesetzt und auf sie sollte nur lesend zugegriffen 
 * werden. 
 *
 *******************************************************************************/

extern int	gl_apid, gl_phys_handle, gl_vdi_handle;
/* 
 * Handle der AES- und VDI-WS
 */
 
extern GRECT	gl_desk;	
/*
 * Grîûe des Desktop
 */
 
/*
 * Breite und Hîhe eines Zeichens im Systemzeichensatz
 */

extern int	sys_bigID, sys_bigHeight,
				sys_smlID, sys_smlHeight;
extern int	sys_wchar, sys_hchar, sys_wbox, sys_hbox;
/*
 * Die Daten des Systemzeichensatzes.
*/

extern bool	gl_gdos;
extern int	gl_font_anz;
extern int	gl_planes;
/*
 * GDOS vorhanden bzw. Anzahl der Fonts auf vdi_handle.
*/

extern bool	gl_mint;
extern int	gl_magx;
/*
 * Die Variablen zeigen an, ob das jeweilige System aktiv ist.
 * gl_magx enthÑlt ggf. die Version.
 * Sie werde beim init_app gesetzt.
 */

extern char gl_appdir[];
/*
 * Pfad, von der die Applikation gestartet wurde.
*/

/*******************************************************************************
 *
 * Einige nÅtzlichen Funktionen		(app.c)
 *
 *******************************************************************************/
 
extern void	init_app(char *rsc, int rbutTree);
/*
 * Meldet die Applikation beim AES und VDI an. Setzt alle o.g.
 * Variable und schaltet Maus auf 'Pfeil'.
 *	rsc: 			Name der zu ladenden Resource oder NULL
 * rbutTree:	Objektbaum-Nr mit den USERDEF-Radioknîpfen oder -1.
 *
 */
 
extern void	exit_gem(void);
/*
 * Meldet die Applikation beim GEM ab, gibt eine eventuell
 * geladene Resource frei, schlieût VDI...
 *
*/

extern void	exit_app(int ret);
/*
 * Ruft exit_gem() und anschlieûend exit(ret) auf.
 */
 
extern void	hide_mouse(void);
/* 
 * Schaltet die Maus ab.
 */

extern void hide_mouse_if_needed(GRECT *rect);
/*
 * Schaltet Maus nur aus, wenn sie im Rechteck liegt.
*/
 
extern void	show_mouse(void);
/*
 * Schaltet die Maus an.
 */

extern bool select_file(char *title, char *path, char *default_name, char *name);
/*
 * Dateiauswahl aufrufen.
 * 'path' enthÑlt den Pfad ohne Wildcard und endet mit '\\'!
 * 'default_name' wird in der Box vorselektiert.
 * 
 * RÅckgabe:
 * 'name' enthÑlt das Ergebnis (Pfad+Name)
 * 'path' enthÑlt den Pfad von 'name'.
 */
 
extern int	appl_xgetinfo(int type, int *out1, int *out2, int *out3, int *out4);

extern int	open_vwork(int *w_out);

/*******************************************************************************
 *
 * Cookie Verwaltung						(cookie.c)
 *
 * Routinen zum Auslesen von Cookies.
 *
 *******************************************************************************/

extern bool	getcookie(char *cookie, long *value);
/*
 * PrÅft auf den angegebenen Cookie.
 *	cookie	: Name des zu ÅberprÅfenden Cookies.
 *	value	: Inhalt des Cookie (falls vorhanden!)
 *
 * RÅckgabe
 *	TRUE	: Cookie gefunden.
 *	FALSE	: Fehler.
 */

/*******************************************************************************
 *
 * Debugging								(debug.c) 
 *
 *******************************************************************************/

typedef enum {null, Con, TCON, Datei, Terminal, 
				  Modem1, Modem2, Seriell1, Seriell2, Prn} DEVICETYP;
/*
 * Legt die mîglichen AusgabekanÑle fest.
 */

extern bool	gl_debug;
/*
 * TRUE nach einem DebugInit().
*/

extern void	DebugInit(char *prgName, DEVICETYP dev, char *file);
/*
 * Initialisiert das Debugdevice.
 * prgName: Wird jeder Debug-Meldng vorangestellt.
 *	dev	 : Typ des Devices
 * file	 : Wenn dev = Datei, der Dateiname der Log-Datei, sonst NULL.
 */
 
extern void	DebugExit(void);
/*
 * Schlieût das Device.
 */
 
extern void	Debug(char *FormatString, ...);
/*
 * Ein Ersatz fÅr fprintf. Parameter so wie bei fprintf.
 */

/*******************************************************************************
 *
 * Drag&Drop protokoll					(dragdrop.c) 
 *
 *******************************************************************************/

#define DD_OK			0
#define DD_NAK			1
#define DD_EXT			2
#define DD_LEN			3
#define DD_TIMEOUT	4000
#define DD_NUMEXTS	8
#define DD_EXTSIZE	32L
#define DD_NAMEMAX	128
#define DD_HDRMAX		(8+DD_NAMEMAX)

extern int	ddcreate(int apid, int winid, int msx, int msy, int kstate, char *exts);
extern int	ddstry(int fd, char *ext, char *name, long size);
extern void	ddclose(int fd);
extern int	ddopen(int ddnam, char *preferext);
extern int	ddrtry(int fd, char *name, char *whichext, long *size);
extern int	ddreply(int fd, int ack);

/*******************************************************************************
 *
 * Dateioperationen						(file.c) 
 *
 *******************************************************************************/

extern bool file_exists(const char *filename);
extern bool path_exists(const char *pathname);
/*
 * PrÅft das Vorhandensein.
*/

extern bool get_path(char *path, char drive);
/*
 * Liefer den aktuellen Pfad des angegebenen Laufwerks. 0 fÅr Defaultdrive.
*/

extern bool make_normalpath(char *path);
/*
 * Wandelt Pfad in einen gÅltigen um (ggf. mit unx2dos).
*/

extern void file_splitt(const char *fullname, char *path, char *name);
/*
 * Spaltet einen kompletten Dateipfad in Pfad und Name auf.
*/

extern void make_shortpath(const char *path, char *shortpath, int maxlen);
/*
 * KÅrzt <path> auf <maxlen>. Falls <path> zulang ist, werden '..' eingefÅgt.
*/

/*******************************************************************************
 *
 * Fontauswahl.							(fontsel.c)
 *
 *******************************************************************************/

extern bool fontsel_exinput(int vdiHandle, int ftype, char *title, 
									 int *fretid, int *fretsize);
/*
 * FÅhrt die modale Fontauswahl Åber den xUFSL-Cookie durch.
 * Benutzt dazu die gleichnamige Funktion aus dem Cookie.
*/

/*******************************************************************************
 *
 * Dialogroutinen							(form_do.c) 
 *
 *******************************************************************************/

extern int	cf_form_do(OBJECT *tree, int ed_start);
/*
 * form_do() Ersatz mit Sondertastenauswertung.
*/

extern int 	simple_dial(OBJECT *tree, int start_edit);
/*
 * FÅhrt einen simplen Dialog durch.
*/

/*******************************************************************************
 *
 * MagiC										(magx.c) 
 *
 *******************************************************************************/

extern int  magxV;
extern int	get_magxV(void);
/*
 * Liefert die MagiX-Version zurÅck.
 * Wird von init_gem aufgerufen!
 */

/*******************************************************************************
 *
 * Objektmanipulation					(objc.c) 
 *
 *******************************************************************************/

extern int 	get_obtype(OBJECT *tree, int obj, int *xtype);
extern long get_obspec(OBJECT *tree, int obj);

extern void set_string(OBJECT *tree, int obj, char *text);
extern void get_string(OBJECT *tree, int obj, char *text);
/*
 * Objekttext setzen/auslesen. Funkt fÅr G_BUTTON, G_STRING, G_TITLE, G_CICON,
 * G_ICON, G_TEXT, G_BOXTEXT, G_FTEXT, G_FBOXTEXT.
 */

extern void set_state(OBJECT *tree, int obj, int state, bool set);
extern bool get_state(OBJECT *tree, int obj, int state);

extern void set_flag(OBJECT *tree, int obj, int flag, bool set);
extern bool get_flag(OBJECT *tree, int obj, int flag);

extern int	find_flag(OBJECT *tree, int flag);
/* Sucht Object in tree mit flag */

extern void get_objframe(OBJECT *tree, int obj, GRECT *r);
/*
 * Ermittelt die absoluten Ausmaûe eines Objekts.
 * Beachtet 3D und alle anderen Flags.
*/

/*******************************************************************************
 *
 * Popupverwaltung.						(popup.c)
 *		
 * Ziel dieses Moduls ist es, dynamisch im Speicher einen 
 * Objektbaum aufzubauen und diesen durch die Funktion
 * menu_popup abarbeiten zu lassen.
 * Da dazu eine neue Funktion des AES benîtigt wird,
 * kann dieses Modul nur unter MultiTOS und Mag!C verwendet 
 * werden!
 *
 *******************************************************************************/
 
typedef struct _popup
{
	OBJECT	*tree;		/* der Objektbaum */
	int	max_item;	/* maximal mîgliche Anzahl */
	int	akt_item;	/* aktuelle Anzahl */
	int	item_len;	/* LÑnge eines Eintrages */
} POPUP;


extern int	create_popup(POPUP *p, int anz, char *item);
/*
 * Neues Popup anlegen. 
 *	p	: die Popup Variable
 *	anz	: die Anzahl der maximal mîglichen EintrÑge
 *	item	: der erste Eintrag, der die LÑnge fÅr alle
 *		  weiteren festlegt!!
 * RÅckgabe
 *	TRUE	: alles OK
 *	FALSE	: Zuwenig freier Speicher
 */

extern int	free_popup(POPUP *p);
/*
 * Gibt den Speicher wieder frei.
 *	p	: das Popup
 *
 * RÅckgabe
 *	TRUE	: alles OK
 *	FALSE	: `p` war schon gelîscht
 */
 
extern int	append_popup(POPUP *p, char *item);
/*
 * Eintrag am Ende anhÑngen.
 *	p	: das Popup
 *	item	: Eintrag, der am Ende angehÑngt werden soll. Wenn ein
 *		  Eintrag aus '-' besteht, wird er 'disabled' angezeigt.
 *
 * RÅckgabe
 *	TRUE	: alles OK
 *	FALSE	: Kein Eintrag mehr frei
 */
  
extern int	do_popup(POPUP *p, int button);
/*
 * Popup auf den Bildschirm bringen und ausfÅhren. Es erscheint
 * an der aktuellen Mausposition.
 *	p	: das popup
 *
 * RÅckgabe
 *	Der gewÑhlte Eintrag (beginnend bei 1) oder null.
 */

extern int	cf_menu_popup(MENU *m1, int x, int y, MENU *m2, int button);
/*
 * Kopie, die enweder menu_popup() aufruft oder diesen Call emuliert.
 * Die Emulation unterstÅtzt keine scrollende MenÅs!!
 */

/*******************************************************************************
 *
 * Userdef-Verwaltung					(userdef.c) 
 *
 *******************************************************************************/

extern void fix_dial(OBJECT *tree);
/*
 * USERDEFs in dem Baum suchen und Zeichenroutinen anmelden.
*/

extern void fix_menu(OBJECT *tree);
/*
 * Ersetzt die '--' durch echte Linie.
 * Kann auch fÅr die Trenner in Popups benutzt werden.
*/

/*******************************************************************************
 *
 * Fensterdialoge							(wdials.c)
 *
 *******************************************************************************/

/* wdial->mode */
#define WD_OPEN	1
#define WD_ICON	2
#define WD_SHADE	3
#define WD_CLOSER	0xFF		/* exit_obj fÅr Closer, fall kein UNDO-Obj vorhandne */

typedef struct _wdial
{
	struct _wdial	*next;

	OBJECT	*tree;						/* Objektbaum */
	OBJECT	*icon;						/* Icon fÅr Iconify */
	int		mode;							/* aktueller Status */
	int		win_handle;					/* Fensterhandle */
	char		win_name[80];				/* Fenstertitel */
	GRECT		work;							/* Fenstergrîûe */
	int		title_obj;					/* Objektnummer des Titelobjektes */
	int		cancel_obj;					/*       "      des Abbruchbuttons */
	int		delta_y;						/* Offset bis zum Titelobjekt */
	int		edit_idx,					/* Objektnummern fÅr die Editfelder */
				next_obj,
				edit_obj;

	void		(*open_cb) (struct _wdial *dial);	
	/*
	 * Wird aufgerufen, bevor das Fenster geîffnet wird.
	*/

	bool		(*exit_cb) (struct _wdial *dial, int exit_obj);	
	/* 
	 * Wird angesprungen, wenn ein EXIT-Objekt betÑtigt wurde. Der Dialog
	 * wird geschlossen aber nicht gelîscht, sobald TRUE zurÅckgeliefert wird.
	 * Wurde der Closer betÑtigt, wird der Index des UNDO-Buttons (Flag 11)
	 * benutzt. Wenn kein UNDO-Button vorhanden, WD_CLOSER. 
	*/ 
} WDIALOG;

typedef void (*WDIAL_OCB)(WDIALOG *dial);
typedef bool (*WDIAL_XCB)(WDIALOG *dial, int exit_obj);

extern WDIALOG	*create_wdialog(OBJECT *tree, OBJECT *icon, int edit_obj, 
											WDIAL_OCB open_cb, WDIAL_XCB exit_cb);
extern void 	delete_wdialog(WDIALOG *wd);
extern void 	open_wdialog(WDIALOG *wd, int pos_x, int pos_y);
extern void 	close_wdialog(WDIALOG *wd);
extern void		redraw_wdobj(WDIALOG *wd, int obj, int depth);
extern void		redraw_wdicon(WDIALOG *wd, int obj, int depth);

extern bool		message_wdialog(int *msg);
extern bool		click_wdialog(int clicks, int x, int y, int kshift, int mbutton);
extern bool		key_wdialog(int kreturn, int kstate);

/*******************************************************************************
 *
 * Sonstiger Kram							(misc.c)
 *
 *******************************************************************************/
extern long	ts2ol(int i1, int i2);
extern void ol2ts(long l, int *i1, int *i2);
/*
 * "two shorts to one long"
 * und
 * "one long to two shorts"
*/

extern void save_background		(GRECT *box, MFDB *buffer);
extern void restore_background	(GRECT *box, MFDB *buffer);
/*
 * Bildschirmhintergrund puffern.
*/
 
#endif
