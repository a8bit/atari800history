#ifndef __DEVICES__
#define __DEVICES__

void Device_Initialise (int *argc, char *argv[]);

void Device_HHOPEN (void);
void Device_HHCLOS (void);
void Device_HHREAD (void);
void Device_HHWRIT (void);
void Device_HHSTAT (void);
void Device_HHSPEC (void);
void Device_HHINIT (void);

void Device_PHOPEN (void);
void Device_PHCLOS (void);
void Device_PHREAD (void);
void Device_PHWRIT (void);
void Device_PHSTAT (void);
void Device_PHSPEC (void);
void Device_PHINIT (void);

void Device_KHOPEN (void);
void Device_KHCLOS (void);
void Device_KHREAD (void);
void Device_KHWRIT (void);
void Device_KHSTAT (void);
void Device_KHSPEC (void);
void Device_KHINIT (void);

void Device_SHOPEN (void);
void Device_SHCLOS (void);
void Device_SHREAD (void);
void Device_SHWRIT (void);
void Device_SHSTAT (void);
void Device_SHSPEC (void);
void Device_SHINIT (void);

void Device_EHOPEN (void);
void Device_EHCLOS (void);
void Device_EHREAD (void);
void Device_EHWRIT (void);
void Device_EHSTAT (void);
void Device_EHSPEC (void);
void Device_EHINIT (void);

#endif
