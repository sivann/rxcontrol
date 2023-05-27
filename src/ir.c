/*
 * Copyright (C) 1999 Spiros Ioannou <sivann@softlab.ece.ntua.gr>
 *
 * ir.c - Infrared communication and decoding stuff. 
 *        Needs lircd daemon to be started.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>

#include "ir.h"

/*extern IRmap keymap[IRKEYS];*/

int 
ir_init()
{
  int irsd;
  struct sockaddr_un addr;

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, LIRCD_SOCKET);

  irsd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (irsd == -1) {
    perror("ir_init:socket");
    exit(errno);
    //return -1;
  };

  if (connect(irsd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    perror("ir_init:connect");
    fprintf(stderr,"ir_init:is lircd running?\n");
    exit(errno);
    //return -1;
  };
  return irsd;
}

void
ir_parsebuf(IR * irs) {
  int i=0;
  sscanf(irs->buf,"%x%x%s%s",&(irs->code),&irs->serial,irs->key,irs->remote);

  while ((strcmp(keymap[i].irkey,irs->key)) && i<(IRKEYS-1)) i++;

  if ((!strcmp(keymap[i].irkey,irs->key)))
    irs->rxkey=keymap[i].rxkey;
  else 
    irs->rxkey=0;
}

/*
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int
ir_main()
{
  int irsd,i;
  fd_set  inout_fds;
  IR irdata;

  irsd=ir_init();
  ir_loadkeys();
  
  for(;;) {
    FD_ZERO(&inout_fds);
    FD_SET(irsd,&inout_fds);
    i=select(16,&inout_fds,(fd_set *) 0, (fd_set *) 0, 0);

    if (FD_ISSET(irsd,&inout_fds)) {
      i = read(irsd, irdata.buf, sizeof(irdata.buf));
      ir_parsebuf(&irdata);
      printf("key:%s,serial:%d, rxkey:%c\n",
	  irdata.key,irdata.serial,irdata.rxkey);
    }
  }
}
*/
