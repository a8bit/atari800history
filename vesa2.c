/* routines with VESA2 support for Atari800 emulator
   9.5.98 Robert Golias    golias@fi.muni.cz
*/
#include <go32.h>
#include <dpmi.h>
#include <sys/farptr.h>

typedef unsigned short int UWORD;
typedef unsigned char UBYTE;
typedef unsigned int ULONG;

#define TRUE 1
#define FALSE 0

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
/*note: the blitting routines are written specifically for atari emulator
  and may presume things, that are not true for general purpose
  blitting routines!! */

