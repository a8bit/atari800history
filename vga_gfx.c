/* graphics routines for Atari800 emulator
   1.6.98 Robert Golias    golias@fi.muni.cz
*/
#include <go32.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include "atari.h"

#ifdef AT_USE_ALLEGRO
#include <allegro.h>
#endif


#define TRUE 1
#define FALSE 0

#ifndef AT_USE_ALLEGRO

/***************************************************************************
 * VESA2 support                                                           *
 ***************************************************************************/

/*structure that is returned by VESA function 4f00  (get vesa info)  */
struct VESAinfo
{
  char sig[4];
  UWORD version;
  ULONG OEM; /*dos pointer*/
  ULONG capabilities;
  ULONG videomodes; /*dos pointer*/
  UWORD memory;
  UWORD oemRevision;
  ULONG oemVendorName; /*dos pointer*/
  ULONG oemProductName; /*dos pointer*/
  ULONG oemProductRev; /*dos pointer*/
  UBYTE reserved[222+256];
}__attribute__((packed));
/*structure that is returned by VESA function 4f01 (get videomode info) */
struct modeInfo
{
  UWORD attributes;
  UBYTE winAAttributes;
  UBYTE winBAttributes;
  UWORD winGranularity;
  UWORD winSize;
  UWORD winASegment;
  UWORD winBSegment;
  ULONG winfunc; /*dos pointer*/
  UWORD bytesPerScanline;

  UWORD XResolution;
  UWORD YResolution;
  UBYTE XCharSize;
  UBYTE YCharSize;
  UBYTE numOfPlanes;
  UBYTE bitsPerPixel;
  UBYTE numOfBanks;
  UBYTE memoryModel;
  UBYTE bankSize;
  UBYTE numOfImagePages;
  UBYTE reserved;

  UBYTE redMaskSize;
  UBYTE redFieldPosition;
  UBYTE greenMaskSize;
  UBYTE greenFieldPosition;
  UBYTE blueMaskSize;
  UBYTE blueFieldPosition;
  UBYTE reservedMaskSize;
  UBYTE reservedPosition;
  UBYTE directAttributes;

  ULONG basePtr;
  ULONG offScreenMemoryOffset;
  ULONG offScreenMemorySize;
  UBYTE reserved2[216];
}__attribute__((packed));


/*functions for mapping physical memory and allocating the corresponding selector */
UBYTE mapPhysicalMemory(ULONG addr,ULONG length,ULONG *linear)
{
  __dpmi_meminfo meminfo;

  if (addr>=0x100000)
  {
    meminfo.size=length;
    meminfo.address=addr;
    *linear=0;
    if (__dpmi_physical_address_mapping(&meminfo)!=0)
      return FALSE;
    *linear=meminfo.address;
    __dpmi_lock_linear_region(&meminfo);
  }
  else
    *linear=addr;  /* there is no way for mapping memory under 1MB with dpmi0.9
                      so we suppose that the address remains the same*/
  return TRUE;
}
UBYTE unmapPhysicalMemory(ULONG *linear)
{
  __dpmi_meminfo meminfo;

  if (*linear>=0x100000)
  {
    meminfo.address=*linear;
    __dpmi_free_physical_address_mapping(&meminfo);
  }  //when <1MB, suppose that it lies in regular DOS memory and does not
     //requires freeing...
  *linear=0;
  return TRUE;
}
UBYTE createSelector(ULONG addr,ULONG length,int *selector)
{
  *selector=__dpmi_allocate_ldt_descriptors(1);
  if (*selector<0)
  {
    *selector=0;
    return FALSE;
  }
  if (__dpmi_set_segment_base_address(*selector,addr)<0)
  {
    __dpmi_free_ldt_descriptor(*selector);
    return FALSE;
  }
  if (__dpmi_set_segment_limit(*selector,length-1)<0)
  {
    __dpmi_free_ldt_descriptor(*selector);
    return FALSE;
  }
  return TRUE;
}
UBYTE freeSelector(int *selector)
{
  if (*selector>0)
  {
    __dpmi_free_ldt_descriptor(*selector);
    *selector=0;
  }
  return TRUE;
}

UBYTE getPhysicalMemory(ULONG addr,ULONG length,ULONG *linear,int *selector)
{
  if (!mapPhysicalMemory(addr,length,linear))
  {
    selector=0;
    return FALSE;
  }
  if (!createSelector(*linear,length,selector))
  {
    unmapPhysicalMemory(linear);
    return FALSE;
  }
  return TRUE;
}
UBYTE freePhysicalMemory(ULONG *linear,int *selector)
{
  unmapPhysicalMemory(linear);
  linear=0;
  selector=0;
  return TRUE;
}

/* VESA getmode - finds the desired videomode*/
UBYTE VESA_getmode(int width,int height,UWORD *videomode,ULONG *memaddress,ULONG *line,ULONG *memsize)
{
  __dpmi_regs rg;
  struct VESAinfo vInfo;
  struct modeInfo mInfo;
  UWORD modesAddr;
  UWORD mode;

  *videomode=0;

  if (_go32_info_block.size_of_transfer_buffer < 512)
    return FALSE;  /*transfer buffer too small*/

  memset(&vInfo,0,sizeof(vInfo));
  strncpy(vInfo.sig,"VBE2",4);
  dosmemput(&vInfo,sizeof(vInfo),__tb&0xfffff);
  rg.x.ax=0x4f00;
  rg.x.di=__tb & 0xf;
  rg.x.es=(__tb >>4 )&0xffff;
  __dpmi_int(0x10,&rg);
  dosmemget(__tb&0xfffff,sizeof(vInfo),&vInfo);
  if (rg.x.ax!=0x004f || strncmp(vInfo.sig,"VESA",4) || (vInfo.version<0x200) )
    return FALSE;  /*vesa 2.0 not found*/

  /*now we must search the videomode list for desired screen resolutin*/
  modesAddr=( (vInfo.videomodes>>12)&0xffff0)+vInfo.videomodes&0xffff;
  while ((mode=_farpeekw(_dos_ds,modesAddr))!=0xffff)
  {
    modesAddr+=2;

    rg.x.ax=0x4f01;
    rg.x.cx=mode;
    rg.x.es=(__tb>>4) & 0xffff;
    rg.x.di=__tb & 0xf;
    __dpmi_int(0x10,&rg);   /*get vesa-mode info*/
    if (rg.h.al!=0x4f) return FALSE;  /*function not supported, but we need it*/


    if (rg.h.ah!=0) continue; /*mode not supported*/
    dosmemget(__tb&0xfffff,sizeof(mInfo)-216,&mInfo); /*note: we don't need the reserved bytes*/
    if (mInfo.attributes&0x99 != 0x99) continue;
    /*0x99 = available, color, graphics mode, LFB available*/
    if (mInfo.XResolution!=width || mInfo.YResolution!=height) continue;
    if (mInfo.numOfPlanes!=1 || mInfo.bitsPerPixel!=8 || mInfo.memoryModel!=4) continue;
    break;
  }

  if (mode==0xffff) return FALSE;

  *line=mInfo.bytesPerScanline;
  *memsize=*line * height;
  *videomode=(mode&0x03ff)|0x4000;  /*LFB version of video mode*/
  *memaddress=mInfo.basePtr;
  return TRUE;
}

/* VESA_open - opens videomode and allocates the corresponding selector */
UBYTE VESA_open(UWORD mode,ULONG memaddress,ULONG memsize,ULONG *linear,int *selector)
{
  __dpmi_regs rg;

  rg.x.ax=0x4f02;
  rg.x.bx=mode;
  __dpmi_int(0x10,&rg);
  if (rg.x.ax!=0x004f) return FALSE;

  if (!getPhysicalMemory(memaddress,memsize,linear,selector))
    return FALSE;
  return TRUE;
}
UBYTE VESA_close(ULONG *linear,int *selector)
{
  __dpmi_regs rg;

  freePhysicalMemory(linear,selector);
  rg.x.ax=0x0003;
  __dpmi_int(0x10,&rg);
  return TRUE;
}

/* bitmap blitting function */
void VESA_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector)
{
  __asm__ __volatile__(
    "
      pushw %%es
      movw %5,%%es
      movl %0,%%esi
      movl $0,%%edi
      movl %1,%%ebx
      subl %%ebx,%3
      subl %%ebx,%4
      shrl $4,%%ebx
      movl %2,%%ecx
0:
      movl %%ebx,%%eax
1:
      movsl
      movsl
      movsl
      movsl
      decl %%eax
      jne 1b
      addl %4,%%edi
      addl %3,%%esi
      dec %%ecx
      jne 0b
      popw %%es
    "
    :
    : "g"(mem), "g"(width), "g"(height), "g"(bitmapline), "g"(videoline), "a"(selector)
    : "%eax", "%ebx", "%ecx", "%esi", "%edi"
  );
}

/* interlaced bitmap blitting function */

void VESA_i_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector)
{
  static UBYTE lcount;
  __asm__ __volatile__(
    "
      pushw %%es
      movw %5,%%es
      movl %0,%%esi
      movl $0,%%edi
      movl %1,%%ebx
      movl %4,%%edx
      subl $4,%%edx
      shll $1,%4
      subl %%ebx,%3
      subl %%ebx,%4
      shrl $3,%%ebx
      movb %%bl,%6
      movl %2,%%ecx
0:
      movb %6,%%ch
1:
      lodsl
      stosl
      movl %%eax,%%ebx
      andl $0xf0f0f0f0,%%eax
      shrl $1,%%ebx
      andl $0x07070707,%%ebx
      orl %%ebx,%%eax
      .BYTE 0x26
      movl %%eax,(%%edi,%%edx)
      lodsl
      stosl
      movl %%eax,%%ebx
      andl $0xf0f0f0f0,%%eax
      shrl $1,%%ebx
      andl $0x07070707,%%ebx
      orl %%ebx,%%eax
      .BYTE 0x26
      movl %%eax,(%%edi,%%edx)
      decb %%ch
      jne 1b
      addl %4,%%edi
      addl %3,%%esi
      decb %%cl
      jne 0b
      popw %%es
    "
    :
    : "g"(mem), "g"(width), "g"(height), "g"(bitmapline), "g"(videoline), "a"(selector), "g"(lcount)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
  );
}




/***************************************************************************
 * XMODE support                                                           *
 ***************************************************************************/

/* Procedure for initializing X-mode. Possible modes:
 *              0       320x175                 6       360x175
 *              1       320x200                 7       360x200
 *              2       320x240                 8       360x240
 *              3       320x350                 9       360x350
 *              4       320x400                 a       360x400
 *              5       320x480                 b       360x480
 *
 * This procedure is based on Guru mode by Per Ole Klemetsrud
 */
UWORD x_regtab[]={0x0e11,0x0014,0xe317,0x6b00,0x5901,
                  0x5a02,0x8e03,0x5e04,0x8a05,0x2d13};
UWORD x_vert480[]={0x4009,0x3e07,0x0b06,0xea10,0x8c11,
                   0xe715,0x0416,0xdf12,0xdf12};
UWORD x_vert350[]={0x4009,0x1f07,0xbf06,0x8310,0x8511,
                   0x6315,0xba16,0x5d12,0x5d12};
UBYTE x_clocktab[]={0xa3,0x0,0xe3,0xa3,0x0,0xe3,0xa7,0x67,
                    0xe7,0xa7,0x67,0xe7};
UWORD *x_verttab[]={x_vert350+1,(void*)0,x_vert480+1,x_vert350,(void*)1,x_vert480,
                   x_vert350+1,(void*)0,x_vert480+1,x_vert350,(void*)1,x_vert480};

UBYTE x_open(UWORD mode)
{
    __dpmi_regs rg;

    if (mode>0xb) return 0;
    rg.x.ax = 0x1a00;
    __dpmi_int(0x10,&rg);
    if (rg.h.al != 0x1a) return 0;

    rg.x.ax=0x0013;
    __dpmi_int(0x10,&rg);

     __asm__ __volatile__("
     pushw %%es

     movzwl %0,%%ebx
     movw %1,%%es
     movw $0x03c4,%%dx
     movb _x_clocktab(%%ebx),%%cl
     orb %%cl,%%cl
     je x_Skip_Reset

     movw $0x100,%%ax
     outw %%ax,%%dx

     movw $0x3c2,%%dx
     movb %%cl,%%al
     outb %%al,%%dx

     movw $0x3c4,%%dx
     movw $0x300,%%ax
     outw %%ax,%%dx
x_Skip_Reset:
     movw $0x604,%%ax
     outw %%ax,%%dx

     cld
     movl $0xa0000,%%di
     xorl %%eax,%%eax
     mov $0x8000,%%cx
     rep
     stosw

     movw $0x3d4,%%dx
     xorl %%esi,%%esi
     movl %%esi,%%ecx
     movw $3,%%cx
     cmpb $5,%%bl
     jbe x_CRT1
     movw $10,%%cx
x_CRT1:
     movw _x_regtab(,%%esi,2),%%ax
     outw %%ax,%%dx
     incl %%esi
     decw %%cx
     jne x_CRT1

     movl _x_verttab(,%%ebx,4),%%esi
     cmpl $0,%%esi
     je x_noerr
     cmpl $1,%%esi
     je x_Set400

     movw $8,%%cx
x_CRT2:
     movw (%%esi),%%ax
     outw %%ax,%%dx
     incl %%esi
     incl %%esi
     decl %%ecx
     jne x_CRT2
     jmp x_noerr

x_Set400:
     mov $0x4009,%%ax
     out %%ax,%%dx
x_noerr:
     xorb %%al,%%al
x_exit:
     popw %%es
     "
     :
     : "g" (mode), "c" (_dos_ds)
     : "%esi", "%eax", "%ebx", "%ecx", "%edx"
     );
}

/* Procedure for copying atari screen to x-mode videomemory */
void x_blit(void *mem,UBYTE lines,ULONG change,ULONG videoline)
{
  asm volatile("
     movl %0,%%esi
     pushw %%es
     movw %1,%%es
     subl $4,%3

     movw $0x102,%%bx
     movw $0x3c4,%%dx
     cld

     movb %2,%%ch
     movl $0xa0000,%%edi
.ALIGN 4
__xb_lloop:
     movb $0x11,%%bh
__xb_mloop:
     movw %%bx,%%ax
     outw %%ax,%%dx
     movb $20,%%cl
.ALIGN 4
__xb_loop:
     movb (%%esi),%%al
     movb 4(%%esi),%%ah
     shrd $16,%%eax,%%eax
     movb 8(%%esi),%%al
     movb 12(%%esi),%%ah
     shrd $16,%%eax,%%eax
     addl $16,%%esi
     stosl

     dec %%cl
     jne __xb_loop

     subl $319,%%esi
     subl $80,%%edi
     shlb $1,%%bh
     cmpb $0x10,%%bh
     jne __xb_mloop

     addl %3,%%esi
     addl %4,%%edi
     dec %%ch
     jne __xb_lloop

     popw %%es
    "
    :
    : "b" (mem), "a"(_dos_ds), "g" (lines), "m" (change), "m" (videoline)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
  );
}

/* Procedure for copying atari screen to x-mode videomemory interlaced with darker-lines (-video 3)*/
void x_blit_i2(void *mem,UBYTE lines,ULONG change,ULONG videoline)
{
  asm volatile("
     movl %0,%%esi
     pushw %%es
     movw %1,%%es
     subl $4,%3

     movw $0x102,%%bx
     movw $0x3c4,%%dx
     cld

     movb %2,%%ch
     movl $0xa0000,%%edi
.ALIGN 4
__x2_lloop:
     movb $0x11,%%bh
__x2_mloop:
     movw %%bx,%%ax
     movw $0x3c4,%%dx
     outw %%ax,%%dx
     movb $20,%%cl
.ALIGN 4
__x2_loop:
     movb (%%esi),%%al
     movb 4(%%esi),%%ah
     shrd $16,%%eax,%%eax
     movb 8(%%esi),%%al
     movb 12(%%esi),%%ah
     shrd $16,%%eax,%%eax
     addl $16,%%esi
     movl %%eax,%%edx
     stosl
     shr $1,%%edx
     and $0xf0f0f0f0,%%eax
     and $0x07070707,%%edx
     or %%edx,%%eax
     .BYTE 0x26
     movl %%eax,76(%%edi)

     dec %%cl
     jne __x2_loop

     subl $319,%%esi
     subl $80,%%edi
     shlb $1,%%bh
     cmpb $0x10,%%bh
     jne __x2_mloop

     addl %3,%%esi
     addl %4,%%edi
     dec %%ch
     jne __x2_lloop

     popw %%es
    "
    :
    : "b" (mem), "a"(_dos_ds), "g" (lines), "m" (change), "m" (videoline)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
  );
}

/*note: the blitting routines are written specifically for atari emulator
  and may presume things, that are not true for general purpose
  blitting routines!! */


#else



/***************************************************************************
 * ALLEGRO support                                                         *
 ***************************************************************************/


/*this routine maps allegro's bitmap to given memory */
void Map_bitmap(BITMAP *bitmap,void *memory,int width,int height)
{
  int i;

  bitmap->dat=memory;
  bitmap->line[0]=memory;
  for (i=1;i<height;i++)
    bitmap->line[i]=bitmap->line[i-1]+width;
}
/*this routine maps allegro's bitmap to given memory interlaced with memory2*/
void Map_i_bitmap(BITMAP *bitmap,void *memory,void *memory2,int width,int height)
{
  int i;

  bitmap->dat=memory;
  bitmap->line[0]=memory;
  bitmap->line[1]=memory2;
  for (i=2;i<height;i+=2)
  {
    bitmap->line[i]=bitmap->line[i-2]+width;
    bitmap->line[i+1]=bitmap->line[i-1]+width;
  }
}

/*this routine makes darker copy of buffer 'source' in buffer 'target' */
void make_darker(void *target,void *source,int bytes)
{
  __asm__ ("
          movl %0,%%edi
          movl %1,%%esi
          movl %2,%%ecx
          subl $4,%%edi
          shr $2,%%ecx
0:
          movl (%%esi),%%eax
          movl %%eax,%%ebx
          andl  $0xf0f0f0f0,%%eax
          shrl $1,%%ebx
          addl $4,%%esi
          addl $4,%%edi
          andl $0x07070707,%%ebx
          orl %%ebx,%%eax
          decl %%ecx
          movl %%eax,(%%edi)
          jne 0b
    "
    :
    : "g" (target), "g" (source), "g" (bytes)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"
  );
}

#endif


