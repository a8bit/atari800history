#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *rcsid = "$Id: configure.c,v 1.11 1996/09/29 22:01:07 david Exp $";

void RemoveLF (char *string)
{
  int len;

  len = strlen (string);
  if (string[len-1] == '\n')
    string[len-1] = '\0';
}

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

int main (void)
{
  FILE *fp;
  char config_filename[256];
  char *home;

  char os_dir[256];
  char disk_dir[256];
  char rom_dir[256];
  char h1_dir[256];
  char h2_dir[256];
  char h3_dir[256];
  char h4_dir[256];
  char print_command[256];
  int refresh_rate;
  char linux_joystick;
  char fps_monitor;
  char direct_video;
  int pal;

  home = getenv ("~");
  if (!home)
    home = getenv ("HOME");
  if (!home)
    home = ".";

#ifndef DJGPP
  strcpy (os_dir, "/usr/local/lib/atari/ROMS");
  strcpy (disk_dir, "/usr/local/lib/atari/DISKS");
  strcpy (h1_dir, "/usr/local/lib/atari/H1");
  strcpy (h2_dir, "/usr/local/lib/atari/H2");
  strcpy (h3_dir, "/usr/local/lib/atari/H3");
  strcpy (h4_dir, "/usr/local/lib/atari/H4");
  strcpy (print_command, "lpr %s");
  strcpy (rom_dir, "/usr/local/lib/atari/ROMS");
  refresh_rate = 1;
  linux_joystick = 'N';
  fps_monitor = 'N';
  direct_video = 'N';
  pal = 1;

  sprintf (config_filename, "%s/.atari800", home);
#else
  strcpy (os_dir, "c:/atari800/ROMS");
  strcpy (disk_dir, "c:/atari800/DISKS");
  strcpy (h1_dir, "c:/atari800/H1");
  strcpy (h2_dir, "c:/atari800/H2");
  strcpy (h3_dir, "c:/atari800/H3");
  strcpy (h4_dir, "c:/atari800/H4");
  strcpy (print_command, "copy %s prn");
  strcpy (rom_dir, "c:/atari800/ROMS");
  refresh_rate = 1;
  linux_joystick = 'N';
  fps_monitor = 'N';
  direct_video = 'N';
  pal = 1;

  sprintf (config_filename, "%s/atari800.cfg", home);
#endif

  fp = fopen (config_filename, "r");
  if (fp)
    {
      printf ("\nReading: %s\n\n", config_filename);

      fgets (disk_dir, 256, fp);
      RemoveLF (disk_dir);

      fgets (h1_dir, 256, fp);
      RemoveLF (h1_dir);

      fgets (h2_dir, 256, fp);
      RemoveLF (h2_dir);

      fgets (h3_dir, 256, fp);
      RemoveLF (h3_dir);

      fgets (h4_dir, 256, fp);
      RemoveLF (h4_dir);

      fgets (print_command, 256, fp);
      RemoveLF (print_command);

      fgets (rom_dir, 256, fp);
      RemoveLF (rom_dir);

      if (fscanf(fp,"%d", &refresh_rate) == 0)
	refresh_rate = 1;

      if (fscanf (fp,"\n%c", &linux_joystick) == 0)
        linux_joystick = 'N';

      if (fscanf (fp,"\n%c", &fps_monitor) == 0)
        fps_monitor = 'N';

      if (fscanf (fp,"\n%c\n", &direct_video) == 0)
        direct_video = 'N';

      fgets (os_dir, 256, fp);
      RemoveLF (os_dir);
      if (strlen(os_dir) == 0)
	strcpy (os_dir, "/usr/local/lib/atari");

      if (fscanf(fp,"%d", &pal) == 0)
	pal = 1;

      fclose (fp);
    }

  GetString ("Enter path to OS ROMs [%s] ", os_dir);
  GetString ("Enter path for disk images [%s] ", disk_dir);
  GetString ("Enter path for ROM images [%s] ", rom_dir);
  GetString ("Enter path for H1: device [%s] ", h1_dir);
  GetString ("Enter path for H2: device [%s] ", h2_dir);
  GetString ("Enter path for H3: device [%s] ", h3_dir);
  GetString ("Enter path for H4: device [%s] ", h4_dir);
  GetString ("Enter command to print file [%s] ", print_command);
  GetNumber ("Generate screen every (1-50) frame(s) [%d] ", &refresh_rate);
  if (refresh_rate < 1)
    refresh_rate = 1;
  else if (refresh_rate > 50)
    refresh_rate = 50;
  YesNo ("Enable LINUX Joystick [%c] ", &linux_joystick);
  YesNo ("Enable Frames per Second Monitor [%c] ", &fps_monitor);
  YesNo ("Enable Direct Video Access [%c] ", &direct_video);
  do
    GetNumber ("NTSC (0) or PAL (1) System [%d] ", &pal);
  while ((pal < 0) || (pal > 1));

  fp = fopen ("config.h", "w");
  if (fp)
    {
      fprintf (fp, "#ifndef __CONFIG__\n");
      fprintf (fp, "#define __CONFIG__\n");

      fprintf (fp, "#define ATARI_LIBRARY \"%s\"\n", os_dir);
      fprintf (fp, "#define ATARI_DISK_DIR \"%s\"\n", disk_dir);
      fprintf (fp, "#define ATARI_ROM_DIR \"%s\"\n", rom_dir);
      fprintf (fp, "#define ATARI_H1_DIR \"%s\"\n", h1_dir);
      fprintf (fp, "#define ATARI_H2_DIR \"%s\"\n", h2_dir);
      fprintf (fp, "#define ATARI_H3_DIR \"%s\"\n", h3_dir);
      fprintf (fp, "#define ATARI_H4_DIR \"%s\"\n", h4_dir);
      fprintf (fp, "#define PRINT_COMMAND \"%s\"\n", print_command);
      fprintf (fp, "#define DEFAULT_REFRESH_RATE %d\n", refresh_rate);

      if (linux_joystick == 'Y')
        fprintf (fp, "#define LINUX_JOYSTICK\n");

      if (fps_monitor == 'Y')
        fprintf (fp, "#define FPS_MONITOR\n");

      if (direct_video == 'Y')
        fprintf (fp, "#define DIRECT_VIDEO\n");

      if (pal == 1)
	fprintf (fp, "#define PAL\n");

      fprintf (fp, "#endif\n");

      fclose (fp);
    }

  fp = fopen (config_filename, "w");
  if (fp)
    {
      printf ("\nWriting: %s\n\n", config_filename);

      fprintf (fp, "%s\n", disk_dir);
      fprintf (fp, "%s\n", h1_dir);
      fprintf (fp, "%s\n", h2_dir);
      fprintf (fp, "%s\n", h3_dir);
      fprintf (fp, "%s\n", h4_dir);
      fprintf (fp, "%s\n", print_command);
      fprintf (fp, "%s\n", rom_dir);
      fprintf (fp, "%d\n", refresh_rate);
      fprintf (fp, "%c\n", linux_joystick);
      fprintf (fp, "%c\n", fps_monitor);
      fprintf (fp, "%c\n", direct_video);
      fprintf (fp, "%s\n", os_dir);
      fprintf (fp, "%d\n", pal);

      fclose (fp);
    }
  else
    {
      perror (config_filename);
      exit (1);
    }

  return 0;
}
