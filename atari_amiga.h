#ifndef ATARI_AMIGA_H

LONG DisplayYesNoWindow(void);

LONG InsertROM(LONG CartType);
LONG InsertDisk( LONG Drive );

VOID FreeDisplay(void);
VOID SetupDisplay(void);
VOID Iconify(void);

#endif
