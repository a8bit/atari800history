UBYTE Save_PCX_file(int interlace);
#define Save_PCX(dummy) Save_PCX_file(FALSE)
#define Save_PCX_interlaced() Save_PCX_file(TRUE)
