#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "rt-config.h"
#include "atari.h"
#include "cpu.h"
#include "gtia.h"
#include "sio.h"
#include "list.h"

#define FALSE 0
#define TRUE 1

static char charset[8192];
extern unsigned char ascii_to_screen[128];

unsigned char key_to_ascii[256] =
{
  0x6C,0x6A,0x3B,0x00,0x00,0x6B,0x2B,0x2A,0x6F,0x00,0x70,0x75,0x9B,0x69,0x2D,0x3D,
  0x76,0x00,0x63,0x00,0x00,0x62,0x78,0x7A,0x34,0x00,0x33,0x36,0x1B,0x35,0x32,0x31,
  0x2C,0x20,0x2E,0x6E,0x00,0x6D,0x2F,0x00,0x72,0x00,0x65,0x79,0x7F,0x74,0x77,0x71,
  0x39,0x00,0x30,0x37,0x7E,0x38,0x3C,0x3E,0x66,0x68,0x64,0x00,0x00,0x67,0x73,0x61,

  0x4C,0x4A,0x3A,0x00,0x00,0x4B,0x5C,0x5E,0x4F,0x00,0x50,0x55,0x9B,0x49,0x5F,0x7C,
  0x56,0x00,0x43,0x00,0x00,0x42,0x58,0x5A,0x24,0x00,0x23,0x26,0x1B,0x25,0x22,0x21,
  0x5B,0x20,0x5D,0x4E,0x00,0x4D,0x3F,0x00,0x52,0x00,0x45,0x59,0x9F,0x54,0x57,0x51,
  0x28,0x00,0x29,0x27,0x9C,0x40,0x7D,0x9D,0x46,0x48,0x44,0x00,0x00,0x47,0x53,0x41,

  0x0C,0x0A,0x7B,0x00,0x00,0x0B,0x1E,0x1F,0x0F,0x00,0x10,0x15,0x9B,0x09,0x1C,0x1D,
  0x16,0x00,0x03,0x00,0x00,0x02,0x18,0x1A,0x00,0x00,0x9B,0x00,0x1B,0x00,0xFD,0x00,
  0x00,0x20,0x60,0x0E,0x00,0x0D,0x00,0x00,0x12,0x00,0x05,0x19,0x9E,0x14,0x17,0x11,
  0x00,0x00,0x00,0x00,0xFE,0x00,0x7D,0xFF,0x06,0x08,0x04,0x00,0x00,0x07,0x13,0x01,

  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

unsigned char ascii_to_screen[128] =
{
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
};

int GetKeyPress (UBYTE *screen)
{
  int keycode;

  do
    {
      Atari_DisplayScreen (screen);
      keycode = Atari_Keyboard();
    } while (keycode == AKEY_NONE);

  return key_to_ascii [keycode];
}

void Plot (UBYTE *screen, int fg, int bg, int ch, int x, int y)
{
  int offset = ascii_to_screen[(ch & 0x07f)] * 8;
  int i;
  int j;

  char *ptr;

  ptr = &screen[24*ATARI_WIDTH + 32];

  for (i=0;i<8;i++)
    {
      UBYTE data;

      data = charset[offset++];

      for (j=0;j<8;j++)
        {
          int pixel;

          if (data & 0x80)
            pixel = colour_translation_table[fg];
          else
            pixel = colour_translation_table[bg];

          ptr[(y*8+i) * ATARI_WIDTH + (x*8+j)] = pixel;

          data = data << 1;
        }
    }
}

void Print (UBYTE *screen, int fg, int bg, char *string, int x, int y)
{
  while (*string)
    {
      Plot (screen, fg, bg, *string++, x, y);
      x++;
    }
}

void Box (UBYTE *screen, int fg, int bg, int x1, int y1, int x2, int y2)
{
  int x;
  int y;

  for (x=x1+1;x<x2;x++)
    {
      Plot (screen, fg, bg, 18, x, y1);
      Plot (screen, fg, bg, 18, x, y2);
    }

  for (y=y1+1;y<y2;y++)
    {
      Plot (screen, fg, bg, 124, x1, y);
      Plot (screen, fg, bg, 124, x2, y);
    }

  Plot (screen, fg, bg, 17, x1, y1);
  Plot (screen, fg, bg, 5, x2, y1);
  Plot (screen, fg, bg, 3, x2, y2);
  Plot (screen, fg, bg, 26, x1, y2);
}

void ClearScreen (UBYTE *screen)
{
  int x;
  int y;

  for (y=0;y<ATARI_HEIGHT;y++)
    for (x=0;x<ATARI_WIDTH;x++)
      screen[y*ATARI_WIDTH+x] = colour_translation_table[0];

  for (x=0;x<40;x++)
    for (y=0;y<24;y++)
      Plot (screen, 0x9a, 0x94, ' ', x, y);
}

void TitleScreen (UBYTE *screen, char *title)
{
  int x;

  x = (40 - strlen(title)) / 2;

  Box (screen, 0x9a, 0x94, 0, 0, 39, 2);
  Print (screen, 0x9a, 0x94, title, x, 1);
}

void SelectItem (UBYTE *screen,
                 int fg, int bg,
                 int index, char *items[],
                 int nrows, int ncolumns,
                 int xoffset, int yoffset)
{
  int x;
  int y;

  x = index / nrows;
  y = index - (x * nrows);

  x = x * (40 / ncolumns);

  x += xoffset;
  y += yoffset;

  Print (screen, fg, bg, items[index], x, y);
}

int Select (UBYTE *screen,
            int default_item,
            int nitems, char *items[],
            int nrows, int ncolumns,
            int xoffset, int yoffset,
            int scrollable,
            int *ascii)
{
  int index = 0;

  for (index = 0; index < nitems; index++)
    SelectItem (screen, 0x9a, 0x94, index, items, nrows, ncolumns, xoffset, yoffset);

  index = default_item;
  SelectItem (screen, 0x94, 0x9a, index, items, nrows, ncolumns, xoffset, yoffset);

  for (;;)
    {
      int row;
      int column;
      int new_index;

      column = index / nrows;
      row = index - (column * nrows);

      *ascii = GetKeyPress (screen);
      switch (*ascii)
        {
          case 0x1c : /* Up */
            if (row > 0)
              row--;
            break;
          case 0x1d : /* Down */
            if (row < (nrows-1))
              row++;
            break;
          case 0x1e : /* Left */
            if (column > 0)
              column--;
            else if (scrollable)
              return index + nitems;
            break;
          case 0x1f : /* Right */
            if (column < (ncolumns-1))
              column++;
            else if (scrollable)
              return index + nitems * 2;
            break;
          case 0x20 : /* Space */
          case 0x7e : /* Backspace */
          case 0x7f : /* Tab */
          case 0x9b : /* Select */
            return index;
          case 0x1b : /* Cancel */
            return -1;
          default :
            break;
        }

      new_index = (column * nrows) + row;
      if ((new_index >=0) && (new_index < nitems))
        {
          SelectItem (screen, 0x9a, 0x94, index, items, nrows, ncolumns, xoffset, yoffset);

          index = new_index;
          SelectItem (screen, 0x94, 0x9a, index, items, nrows, ncolumns, xoffset, yoffset);
        }
    }
}

void SelectSystem (UBYTE *screen)
{
  int system;
  int ascii;

  char *menu[5] =
    {
      "Atari OS/A",
      "Atari OS/B",
      "Atari 800XL",
      "Atari 130XE",
      "Atari 5200"
    };

  ClearScreen (screen);
  TitleScreen (screen, "Select System");
  Box (screen, 0x9a, 0x94, 0, 3, 39, 23);

  system = Select (screen, 0, 5, menu, 5, 1, 1, 4, FALSE, &ascii);

  switch (system)
    {
      case 0 :
        Initialise_AtariOSA();
        break;
      case 1 :
        Initialise_AtariOSB();
        break;
      case 2 :
        Initialise_AtariXL();
        break;
      case 3 :
        Initialise_AtariXE();
        break;
      case 4 :
        Initialise_Atari5200();
        break;
      default  :
        break;
    }
}

int FilenameSort (char *filename1, char *filename2)
{
  return strcmp(filename1, filename2);
}

List *GetDirectory (char *directory)
{
  DIR *dp = NULL;
  List *list;

  dp = opendir (directory);
  if (dp)
    {
      struct dirent *entry;

      list = ListCreate();
      if (!list)
        {
          printf ("ListCreate(): Failed\n");
          exit (1);
        }

      while ((entry = readdir(dp)))
        {
          char *filename;

          filename = strdup(entry->d_name);
          if (!filename)
            {
              perror ("strdup");
              exit (1);
            }

          ListAddTail (list, filename);
        }

      closedir (dp);

      ListSort (list, FilenameSort);
    }

  return list;
}

int FileSelector (UBYTE *screen, char *directory, char *full_filename)
{
  List *list;
  int flag = FALSE;

  list = GetDirectory (directory);
  if (list)
    {
      char *filename;
      void free();
      int nitems = 0;
      int item = 0;
      int done = FALSE;
      int offset = 0;
      int nfiles = 0;

#define NROWS 19
#define NCOLUMNS 2
#define MAX_FILES (NROWS * NCOLUMNS)

      char *files[MAX_FILES];

      ListReset (list);
      while (ListTraverse(list, (void*)&filename))
        nfiles++;

      while (!done)
        {
          int ascii;

          ListReset (list);
          for (nitems=0;nitems<offset;nitems++)
            ListTraverse(list, (void*)&filename);

          for (nitems=0;nitems<MAX_FILES;nitems++)
            {
              if (ListTraverse(list, (void*)&filename))
                {
                  files[nitems] = filename;
                }
              else
                break;
            }

          ClearScreen (screen);
          TitleScreen (screen, "Select Disk Image");
          Box (screen, 0x9a, 0x94, 0, 3, 39, 23);

          item = Select (screen, item, nitems, files, NROWS, NCOLUMNS, 1, 4, TRUE, &ascii);
          if (item >= (nitems * 2 + NROWS)) /* Scroll Right */
            {
              if ((offset + NROWS + NROWS) < nfiles)
                offset += NROWS;
              item = item % nitems;
            }
          else if (item >= nitems) /* Scroll Left */
            {
              if ((offset - NROWS) >= 0)
                offset -= NROWS;
              item = item % nitems;
            }
          else if (item != -1)
            {
              sprintf (full_filename, "%s/%s", directory, files[item]);
              flag = TRUE;
              break;
            }
          else
            break;
        }

      ListFree (list, free);
    }

  return flag;
}

void DiskManagement (UBYTE *screen)
{
  char *menu[8] =
    {
      NULL, /* D1 */
      NULL, /* D2 */
      NULL, /* D3 */
      NULL, /* D4 */
      NULL, /* D5 */
      NULL, /* D6 */
      NULL, /* D7 */
      NULL, /* D8 */
    };

  int done = FALSE;
  int dsknum = 0;

  menu[0] = sio_filename[0];
  menu[1] = sio_filename[1];
  menu[2] = sio_filename[2];
  menu[3] = sio_filename[3];
  menu[4] = sio_filename[4];
  menu[5] = sio_filename[5];
  menu[6] = sio_filename[6];
  menu[7] = sio_filename[7];

  while (!done)
    {
      char filename[256];
      int ascii;

      ClearScreen (screen);
      TitleScreen (screen, "Disk Management");
      Box (screen, 0x9a, 0x94, 0, 3, 39, 23);

      Print (screen, 0x9a, 0x94, "D1:", 1, 4);
      Print (screen, 0x9a, 0x94, "D2:", 1, 5);
      Print (screen, 0x9a, 0x94, "D3:", 1, 6);
      Print (screen, 0x9a, 0x94, "D4:", 1, 7);
      Print (screen, 0x9a, 0x94, "D5:", 1, 8);
      Print (screen, 0x9a, 0x94, "D6:", 1, 9);
      Print (screen, 0x9a, 0x94, "D7:", 1, 10);
      Print (screen, 0x9a, 0x94, "D8:", 1, 11);

      dsknum = Select (screen, dsknum, 8, menu, 8, 1, 4, 4, FALSE, &ascii);
      if (dsknum != -1)
        {
          if (ascii == 0x9b)
            {
              if (FileSelector (screen, atari_disk_dir, filename))
                {
                  SIO_Dismount (dsknum+1);
                  SIO_Mount(dsknum+1, filename);
                }
            }
          else
            {
              if (strcmp(sio_filename[dsknum],"Empty") == 0)
                SIO_DisableDrive (dsknum+1);
              else
                SIO_Dismount (dsknum+1);
            }
        }
      else
        done = TRUE;
    }
}

void ui (UBYTE *screen)
{
  static int initialised = FALSE;
  char filename[256];
  int option = 0;
  int done = FALSE;

  char *menu[5] =
    {
      "Select System",
      "Disk Management",
      "Power On Reset",
      "Power Off Reset",
      "Exit Emulator"
    };

  if (!initialised)
    {
      memcpy (charset, &memory[224*256], 8192);
      initialised = TRUE;
    }

  while (!done)
    {
      int ascii;

      ClearScreen (screen);
      TitleScreen (screen, ATARI_TITLE);
      Box (screen, 0x9a, 0x94, 0, 3, 39, 23);

      option = Select (screen, option, 5, menu, 5, 1, 1, 4, FALSE, &ascii);

      switch (option)
        {
          case -1 :
            done = TRUE;
            break;
          case 0 :
            SelectSystem (screen);
            break;
          case 1 :
            DiskManagement (screen);
            break;
          case 2 :
            Warmstart ();
            break;
          case 3 :
            Coldstart ();
            break;
          case 4 :
            Atari800_Exit (0);
            exit (0);
        }
    }
}

