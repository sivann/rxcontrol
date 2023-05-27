/* MP3-O-Phono
 * rxcontrol.h
 * Make sure all paths are absolute!
 */

/* You may need to change theese 
 * to sys/termio(s).h for other unices
 */

#define SORT  /*Sort files alphabetically*/
#define INTRO /*comment this if you don't want anything played at startup*/


/*-------------- nothing should be changed below this line ------------- */

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <syslog.h>

#ifdef linux
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/cdrom.h>
#include <sys/mount.h>
#endif

#ifdef HAVEIR
#include "ir.h"
#endif

#include <termio.h>
#include <termios.h>
#include <stdio.h>

#ifdef linux
#include <linux/vt.h>
#include <linux/cdrom.h>
#include "fstab.h"
#else
#undef CHVT   /*Virtual consoles only on linux systems*/
#endif


#include "readconf.h"
#include "id3.h"
#include "rxkeys.h"


/* functions */

void qsort1(int, int, char **);
int  my_warn(char * sender,char *er, int err_no);
int  my_error(char * sender,char *er, int err_no);
int  enable_info(int);
int  disable_info(int);
int  opentray(void);
int  closetray(void);
int  readinput(void);
int  clp(char *);

void chvt(int);
void diehdl(int);
void usage (void);
void no_wait(int);
void WriteScreen(char (*table)[20]);

char * sstate(int state);

int  slowrite(int, char *, int);
int  kbrate(int,int);               /* keyboard delay and rate */
int  readconf(FILE *,conf_s *);     /* read and parse the configuration file*/
int  id3_main(char *, ID3 *);       /* get the ID3 TAG from a file */
int  plbrowse(void);                /* load/delete a pre-compiled playlist */
int  filesel(void);                 /* select a song from disk */
int  pledit(void);                  /* edit the playlist */
int  menusel(void);                 /* select a submenu */
int  rx_shutdown(void);             /* Shutdown the system */
int  recursive_select(int,char*);   /*recursively select files*/


/* configuration variables */
int contrast;
int scrolldelay;
int scrollstep;
int seekoffset;
int writedelay;
int keydelay;
int keyrate;
int irperceive;

char startdir[64];
char pldir[64];
char rxpath[64];
char port[64];
char vt[64];
char halt_cmd[64];
char intro[64];
char rxsrc[64];
char rxdst[64];
char mcopy[64];
char cdrom[64];
char cdromdir[64];

