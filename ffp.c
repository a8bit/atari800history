#include <stdio.h>
#include <math.h>

static char *rcsid = "$Id: ffp.c,v 1.3 1996/07/18 00:33:05 david Exp $";

#include "cpu.h"
/*
#define DEBUG
*/
extern UBYTE BCDtoDEC[256];
extern UBYTE DECtoBCD[256];

#define FR0 0x00d4
#define FR1 0x00e0
#define FLPTR 0x00fc
#define INBUFF 0x00f3
#define CIX 0x00f2
#define LBUFF 0x580

double GetFloat (unsigned char *memory)
{
  double fval;
  unsigned int temp;
  int sign, exp;

  temp = BCDtoDEC[memory[1]];
  temp *= 100;
  temp += BCDtoDEC[memory[2]];
  temp *= 100;
  temp += BCDtoDEC[memory[3]];
  temp *= 100;
  temp += BCDtoDEC[memory[4]];

  fval = (double)temp / 1000000.0;

  exp = (memory[0] & 0x7f) - 64;
  sign = memory[0] & 0x80;

  if (sign)
    fval = -fval;

  fval *= pow(100.0, exp);

  return fval;
}

void PutFloat (unsigned char *memory, double fval)
{
  int sign;
  int exp;
  int i;

  if (fval < 0.0)
    {
      fval = -fval;
      sign = 0x80;
    }
  else
    {
      sign = 0x00;
    }

  if (fval != 0.0)
    {
      exp = 0x40;

      while (fval >= 100.0)
	{
	  fval /= 100.0;
	  exp++;
	}

      while (fval < 1.0)
	{
	  fval *= 100.0;
	  exp --;
	}
    }
  else
    {
      sign = 0x00;
      exp = 0x00;
    }

  memory[0] = sign | exp;

  for (i=1;i<6;i++)
    {
      int ival = fval;
      memory[i] = DECtoBCD[ival];
      fval -= ival;
      fval *= 100.0;
    }

#ifdef DEBUG
  for (i=0;i<6;i++)
    printf ("%02x ", memory[i]);
  printf ("\n");
#endif
}

void ffp_afp (void)
{
  float fr0;
  int inbuff;
  int cix;
  int C = 1;

#ifdef DEBUG
  printf ("ffp_afp: called\n");
#endif

  inbuff = (memory[INBUFF+1] << 8) | memory[INBUFF];
  cix = memory[CIX];

  sscanf (&memory[inbuff+cix],"%f", &fr0);

  PutFloat (&memory[FR0], fr0);
/*
   ==============================
   Value of CIX should be updated
   ==============================
*/
  while (((memory[inbuff+cix] >= '0') && (memory[inbuff+cix] <= '9'))
	 || (memory[inbuff+cix] == '.'))
    {
      cix++;
      C = 0;
    }

  memory[CIX] = cix;

  if (C)
    SetC;
  else
    ClrC;

#ifdef DEBUG
  printf ("ffp_afp: result = %f, cix = %d\n", fr0, cix);
#endif
}

void ffp_fasc (void)
{
  double fval;
  char *lbuff;
  int lastch;

  fval = GetFloat (&memory[FR0]);

#ifdef DEBUG
  printf ("ffp_fasc: called with fr0 = %f\n", fval);
#endif

  lbuff = &memory[LBUFF];

  sprintf (lbuff,"%f",fval);
  lastch = strlen(lbuff) - 1;
/*
 * =====================
 * Remove trailing zeros
 * =====================
 */
  while (lbuff[lastch] == '0')
    lastch--;
/*
 * =====================
 * Remove trailing point
 * =====================
 */
  if (lbuff[lastch] == '.')
    lastch--;

  lbuff[lastch] |= 0x80;
  memory[0x00f3] = LBUFF & 0xff;
  memory[0x00f4] = (LBUFF >> 8) & 0xff;
}

void ffp_ifp (void)
{
  UWORD word;

#ifdef DEBUG
  printf ("ffp_ifp: called\n");
#endif

  word = (memory[FR0+1] << 8) | memory[FR0];

#ifdef DEBUG
  printf ("Word = %d\n", word);
#endif

  PutFloat (&memory[FR0], (float)word);
}

void ffp_fpi (void)
{
  double fr0;
  UWORD word;

  fr0 = GetFloat (&memory[FR0]);

  if ((fr0 >= 0.0) && (fr0 < 65535.5))
    {
      word = (UWORD)(fr0 + 0.5);

      memory[FR0] = word & 0xff;
      memory[FR0+1] = (word >> 8) & 0xff;

#ifdef DEBUG
      printf ("ffp_fpi: result = %d\n", word);
#endif

      ClrC;
    }
  else
    {
#ifdef DEBUG
      printf ("ffp_fpi: *** error ***\n");
#endif

      SetC;
    }
}

void ffp_fadd (void)
{
  double fr0, fr1;

  fr0 = GetFloat (&memory[FR0]);
  fr1 = GetFloat (&memory[FR1]);

#ifdef DEBUG
  printf ("ffp_add: called with fr0=%f and fr1=%f\n", fr0, fr1);
#endif

  fr0 += fr1;

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_fsub (void)
{
  double fr0, fr1;

  fr0 = GetFloat (&memory[FR0]);
  fr1 = GetFloat (&memory[FR1]);

#ifdef DEBUG
  printf ("ffp_sub: called with fr0=%f and fr1=%f\n", fr0, fr1);
#endif

  fr0 -= fr1;

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_fmul (void)
{
  double fr0, fr1;

  fr0 = GetFloat (&memory[FR0]);
  fr1 = GetFloat (&memory[FR1]);

#ifdef DEBUG
  printf ("ffp_mul: called with fr0=%f and fr1=%f\n", fr0, fr1);
#endif

  fr0 *= fr1;

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_fdiv (void)
{
  double fr0, fr1;

  fr0 = GetFloat (&memory[FR0]);
  fr1 = GetFloat (&memory[FR1]);

#ifdef DEBUG
  printf ("ffp_div: called with fr0=%f and fr1=%f\n", fr0, fr1);
#endif

  fr0 /= fr1;

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_log (void)
{
  double fr0;

  fr0 = GetFloat (&memory[FR0]);

#ifdef DEBUG
  printf ("ffp_log: called with fr0 = %f\n", fr0);
#endif

  fr0 = log(fr0);

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_log10 (void)
{
  double fr0;

  fr0 = GetFloat (&memory[FR0]);

#ifdef DEBUG
  printf ("ffp_log10: called with fr0 = %f\n", fr0);
#endif

  fr0 = log10(fr0);

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_exp (void)
{
  double fr0;

  fr0 = GetFloat (&memory[FR0]);

#ifdef DEBUG
  printf ("ffp_exp: called with fr0 = %f\n", fr0);
#endif

  fr0 = exp(fr0);

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_exp10 (void)
{
  double fr0;

  fr0 = GetFloat (&memory[FR0]);

#ifdef DEBUG
  printf ("ffp_exp10: called with fr0 = %f\n", fr0);
#endif

  fr0 = pow(10.0, fr0);

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_plyevl (void)
{
  double fr0;
  double Z;
  UWORD addr;
  int n;

#ifdef DEBUG
  printf ("ffp_plyevl: called\n");
#endif

  Z = GetFloat (&memory[FR0]);

  addr = (regY << 8) + regX;

#ifdef DEBUG
  printf ("Number of coefficients: %d\n", regA);
  printf ("Address of coefficients: %x\n", addr);
  printf ("Z = %f\n", Z);
#endif

  fr0 = 0.0;

  for (n=regA-1;n>=0;n--)
    {
      double coeff;
      double powZ;

      coeff = GetFloat (&memory[addr]);
      addr += 6;

#ifdef DEBUG
      printf ("Coefficient %d: %f\n", n, coeff);
#endif

      powZ = pow(Z, (float)n);

      fr0 += (coeff * powZ);
    }

  PutFloat (&memory[FR0], fr0);

  ClrC;
}

void ffp_zfr0 (void)
{
#ifdef DEBUG
  printf ("ffp_zfr0: called\n");
#endif

  memory[FR0] = 0x00;
  memory[FR0+1] = 0x00;
  memory[FR0+2] = 0x00;
  memory[FR0+3] = 0x00;
  memory[FR0+4] = 0x00;
  memory[FR0+5] = 0x00;
}

void ffp_zf1 (void)
{
#ifdef DEBUG
  printf ("ffp_zf1: called\n");
#endif

  memory[FR1] = 0x00;
  memory[FR1+1] = 0x00;
  memory[FR1+2] = 0x00;
  memory[FR1+3] = 0x00;
  memory[FR1+4] = 0x00;
  memory[FR1+5] = 0x00;
}

void ffp_fld0r (void)
{
  UWORD addr;

#ifdef DEBUG
  printf ("ffp_fld0r: called\n");
#endif

  addr = (regY << 8) + regX;
  memcpy (&memory[FR0], &memory[addr], 6);

  ClrC;
}

void ffp_fld0p (void)
{
  UWORD addr;

#ifdef DEBUG
  printf ("ffp_fld0p: called\n");
#endif

  addr = (memory[FLPTR+1] << 8) + memory[FLPTR];
  memcpy (&memory[FR0], &memory[addr], 6);

  ClrC;
}

void ffp_fld1r (void)
{
  UWORD addr;

#ifdef DEBUG
  printf ("ffp_fld1r: called\n");
#endif

  addr = (regY << 8) + regX;
  memcpy (&memory[FR1], &memory[addr], 6);

  ClrC;
}

void ffp_fld1p (void)
{
  UWORD addr;

#ifdef DEBUG
  printf ("ffp_fld1r: called\n");
#endif

  addr = (memory[FLPTR+1] << 8) + memory[FLPTR];
  memcpy (&memory[FR1], &memory[addr], 6);

  ClrC;
}

void ffp_fst0r (void)
{
  UWORD addr;

#ifdef DEBUG
  printf ("ffp_fst0r: called\n");
#endif

  addr = (regY << 8) + regX;
  memcpy (&memory[addr], &memory[FR0], 6);

  ClrC;
}

void ffp_fst0p (void)
{
  printf ("ffp: ffp_fst0p not implemented\n");
  exit (1);
}

void ffp_fmove (void)
{
#ifdef DEBUG
  printf ("ffp_fmove\n");
#endif

  memcpy (&memory[FR1], &memory[FR0], 6);
}

