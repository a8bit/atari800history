#include <stdio.h>
#include <ctype.h>

#include "prompts.h"

static char *rcsid = "$Id: prompts.c,v 1.1 1997/03/22 21:48:28 david Exp $";

void GetString (char *message, char *string)
{
  char gash[128];

  printf (message, string);
  gets (gash);
  if (strlen(gash) > 0)
    strcpy (string, gash);
}

void GetNumber (char *message, int *num)
{
  char gash[128];

  printf (message, *num);
  gets (gash);
  if (strlen(gash) > 0)
    sscanf (gash,"\n%d", num);
}

void YesNo (char *message, char *yn)
{
  char gash[128];
  char t_yn;

  do
    {
      printf (message, *yn);
      gets (gash);

      if (strlen(gash) > 0)
	t_yn = gash[0];
      else
	t_yn = ' ';

      if (islower(t_yn))
	t_yn = toupper(t_yn);
    } while ((t_yn != ' ') && (t_yn != 'Y') && (t_yn != 'N'));

  if (t_yn != ' ')
    *yn = t_yn;
}

void RemoveLF (char *string)
{
  int len;

  len = strlen (string);
  if (string[len-1] == '\n')
    string[len-1] = '\0';
}
