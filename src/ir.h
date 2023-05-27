#include "ir_loadkeys.h"

#define LIRCD_SOCKET "/dev/lircd"

typedef struct {
  char buf[128];         /*input */
  unsigned int  code;    /*returned*/
  unsigned int  serial ; /*returned*/
  char key[64];          /*returned*/
  char remote[64];       /*returned*/
  char rxkey;            /*returned. a key from rxkeys.h */
} IR;

typedef struct {
  char *irkey;
  char  rxkey;
} IRmap;


IRmap keymap[IRKEYS];
void ir_loadkeys(void);
int ir_init(void);
void ir_parsebuf(IR *);
