/* ATARI 336x240 pcx screenshot
   Robert Golias, golias@fi.muni.cz
   
   Note: DPI in pcx-header is set to 0. Does anybody know the correct values?
*/

#include <stdio.h>
#include <stdlib.h>

#include "atari.h"
#include "config.h"
#include "colours.h"

static int pcx_no=-1;

#define FALSE 0
#define TRUE 1

int Find_PCX_name()
{
  char filename[30];
  FILE *fr;

  pcx_no++;
  while (pcx_no<10000)
  {
    sprintf(filename,"pict%04i.pcx",pcx_no);
    if ((fr=fopen(filename,"r"))==NULL)
      return pcx_no;   /*file does not exist - we can create it */
    pcx_no++;
  }
  return -1;
}


UBYTE Save_PCX(UBYTE *memory)
{
  char filename[30];
  int i,line;
  FILE *fw;
  int XMax=335;
  int YMax=239;
  int bytesPerLine=336;

  if ((i=Find_PCX_name())<0) return FALSE;
  sprintf(filename,"pict%04i.pcx",i);
  if ((fw=fopen(filename,"wb"))==NULL)
    return FALSE;

  /*write header*/
  fputc(0xa,fw); /*pcx signature*/
  fputc(0x5,fw); /*version 5*/
  fputc(0x1,fw); /*RLE encoding*/
  fputc(0x8,fw); /*bits per pixel*/
  fputc(0,fw);fputc(0,fw);fputc(0,fw);fputc(0,fw); /* XMin=0,YMin=0 */
  fputc(XMax&0xff,fw);fputc(XMax>>8,fw);fputc(YMax&0xff,fw);fputc(YMax>>8,fw);
  fputc(0,fw);fputc(0,fw);fputc(0,fw);fputc(0,fw); /*unknown DPI */
  for (i=0;i<48;i++) fputc(0,fw);  /*EGA color palette*/
  fputc(0,fw); /*reserved*/
  fputc(1,fw); /*number of bit planes*/
  fputc(bytesPerLine&0xff,fw);fputc(bytesPerLine>>8,fw);
  fputc(1,fw);fputc(0,fw); /*palette info - unused */
  fputc((XMax+1)&0xff,fw);fputc((XMax+1)>>8,fw);
  fputc((YMax+1)&0xff,fw);fputc((YMax+1)>>8,fw); /*screen resolution*/
  for (i=0;i<54;i++) fputc(0,fw); /*unused*/

  for (line=0;line<=YMax;line++)
  {
    int count;
    int last;
    int xpos;
    UBYTE *mem;

    mem=memory + ATARI_WIDTH*line + ( (ATARI_WIDTH-bytesPerLine) / 2 - 1);
    xpos=0;
    while (xpos<bytesPerLine)
    {

      last=*mem++;
      xpos++;
      count=1;
      while (*mem==last && xpos<bytesPerLine && count<63)
      {
        mem++;count++;xpos++;
      }
      if (count>1 || (last&0xc0)==0xc0)
      {
        fputc(0xc0 | (count & 0x3f), fw);
        fputc(last&0xff,fw);
      } else fputc(last & 0xff,fw);

    }
  }

  /*write palette*/
  fputc(0xc,fw);
  for (i=0;i<256;i++)
  {
    fputc((colortable[i]>>16)&0xff,fw);
    fputc((colortable[i]>>8)&0xff,fw);
    fputc(colortable[i]&0xff,fw);
  }
  fclose(fw);
  return TRUE;
}

