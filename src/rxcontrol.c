/*
 * rxcontrol.c -- interface to rxaudio/MO LCD/file browser e.t.c.
 * Copyright (C) 1998 Spiros Ioannou <sivann@cs.ntua.gr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library Public License as published by
 * the Free Software Foundation version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library Public License for more details.
 */


char *version = "version 1.8, Nov 1999";	


#include "rxcontrol.h"
#include "lcd.h"


char *files[10240];		/* files read in directory */
char *playlist[10240];		/* self-documented ;-) */
char *plfiles[10240];		/* playlists in directory */
int  fdc, pfd[2], pfd2[2], childpid;	/* file/pipe descriptors */
int  irsd;			/*infra red socket descriptor*/
int  plsi = 0;			/* playlist[plsi] is the next  empty playlist 

				 * position */
int  repeat = 0;		/* flag for repeat */
int  tray_open = 0;		/* cd tray status */

FILE *lfp;			/* LCD fp */
struct termios *termios_p;
char lcd_b[4][20];
char lcd_bp[4][20];
char lcd_t[4][20];

extern int prefix;		/* lcd command prefix  */

#ifdef HAVEIR
extern IRmap keymap[IRKEYS];
IR irdata;			/* infra red data structure*/
#endif

ID3  id3data;
conf_s cs;			/* configuration struct */

/* Sort a table of pointers to strings 
 * qsort1(first element(left),last element(right),string pointer table)
 */
void 
qsort1(register int l, register int r, char *a[])
{
  register int i, j;

  char *w;
  char x[256];

  if (l>=r) return;

  i = l;
  j = r;
  strncpy(x, a[(l + r) / 2], 255);
  while (i <= j) {
    while (strcmp(a[i], x) < 0)
      i++;
    while (strcmp(a[j], x) > 0)
      j--;
    if (i <= j) {
      w = a[i];
      a[i] = a[j];
      a[j] = w;
      i++;
      j--;
    }
  }
  if (l < j)
    qsort1(l, j, a);
  if (i < r)
    qsort1(i, r, a);
}


int 
my_warn(char *sender,char *er, int err_no)
{
  char buf[256];
#ifndef USE_SYSLOG
  FILE *erfp;
#endif

  sprintf(buf, "%s: warning: %s: %s\n",sender,er, strerror(err_no));

#ifdef USE_SYSLOG
  syslog(LOG_USER | LOG_WARNING, "%s",buf);
#else
  erfp = fopen("/tmp/rxwarn", "a");
  if (!erfp)
    exit(-10);
  fprintf(erfp, "%s",buf);
  fprintf(stderr,"%s",buf);
  fflush(erfp);
  fclose(erfp);
#endif
  return err_no;
}

int 
my_error(char * sender,char *er, int err_no)
{
#ifndef USE_SYSLOG
  FILE *erfp;
#endif
  char buf[128];

  sprintf(buf, "%s: error: %s: %s\n",sender,er, strerror(err_no));

#ifdef USE_SYSLOG
  syslog(LOG_USER | LOG_ERR, "%s",buf);
#else
  erfp = fopen("/tmp/rxerr", "a");
  if (!erfp)
    exit(-11);
  fprintf(erfp, "%s",buf);
  fprintf(stderr,"%s",buf);
  fflush(erfp);
  fclose(erfp);
#endif
  exit(err_no);
}

/* SVR4 method */
void 
no_wait(int fd)
{				/* Accept key input a.s.a.p.:don't wait for
				 * Enter */
  struct termio ts;

  ioctl(fd, TCGETA, &ts);
  ts.c_cc[VMIN] = 1;
  ts.c_lflag = ISIG;
  ioctl(fd, TCSETA, &ts);
}

/*Function to be called if there's no LCD */
void 
WriteScreen(char (*table)[20])
{
  int  c, r;
  static unsigned char lcd_tab[4][20];

  if (!memcmp(lcd_tab, table, 80))
    return;			/* don't rewrite same screens */

  /* clear screen for vt100-like */
  /* printf("\033[H\033[2J"); */

  printf("\n********************\n");
  for (r = 0; r < 4; r++) {
    for (c = 0; c < 20; c++) {
      printf("%c", table[r][c] == 126 ? '>' : table[r][c]);
    }
    printf("\n");
    fflush(stdout);
  }
  printf("********************\n\n");
  memcpy(lcd_tab, table, 80);
}

/*read input from either the keyboard or the infra red transmiter*/
int
readinput()
{      
  int r,key=0,i,irinput;
  fd_set inout_fds;
  char buffer[512];

  irinput=0;
  FD_ZERO(&inout_fds);
  FD_SET(fdc, &inout_fds);		/* FROM keyboard */
#ifdef HAVEIR
  FD_SET(irsd,&inout_fds);		/* FROM IR */
#endif
  /* No timeout (interrupt driven) select */
  i = select(16, &inout_fds, (fd_set *) 0, (fd_set *) 0, 0 );

  /* input from IR */
#ifdef HAVEIR
  if (FD_ISSET(irsd,&inout_fds)) {
    i = read(irsd, irdata.buf, sizeof(irdata.buf));
    ir_parsebuf(&irdata);
#ifdef DEBUG1
    printf("key:%s,serial:%d, rxkey:%c\n",
      irdata.key,irdata.serial,irdata.rxkey);
#endif
    irinput=irdata.rxkey;
    /*accept multiple keypresses only for some keys such as FF,FB*/
    if (irdata.serial && irinput!=K_FF && irinput!=K_FB)
      irinput=0;
    /*don't accept all pulses, ignore some to reduce rate*/
    else if (irinput==K_FF || irinput==K_FB) {
      if (irdata.serial%irperceive)
	irinput=0;
    }
#ifdef DEBUG1
    printf("irinput:%c\n",irinput);
#endif
  }
#endif

  /* Input from keyboard or IR*/
  if (FD_ISSET(fdc, &inout_fds)||irinput) {
    if (!irinput) {
      r = read(fdc, buffer, 511);
      key=*buffer;
    }
    else {
      key=irinput;
    }
    irinput=0;
  }
  return key;
}




/*Under Developpment */
int 
opentray()
{
  int  ret = 0, fd;


  fd = open(cdrom, O_RDONLY);
  if (fd < 0)
    my_error("opentray",cdrom, errno);

  ret = ioctl(fd, CDROMEJECT);

  if (ret)
    my_warn("opentray",cdrom, errno);

  if (errno == EBUSY) {
    printf("Trying umount...\n");
    ret = umount(cdromdir);
    if (ret)
      my_warn("opentray","umount", errno);
    else {
      printf("Success unmounting\n");
      (void) update_mtab(cdromdir, NULL);	/* update /etc/mtab */
      ret = ioctl(fd, CDROMEJECT);
      if (ret)
	my_warn("opentray",cdromdir, errno);
    }
  }
  close(fd);
  return ret;

}

/*Under Developpment */
int 
closetray()
{
  int  fd;

  fd = open(cdrom, O_RDONLY);
  close(fd);
  return fd;
}

/*rxaudio state translation to string */
char *
sstate(state)
{
  static char *s = NULL;

  s = realloc(s, 10);
  switch (state) {
  case 0:
    sprintf(s, "     %c", repeat ? '#' : ' ');
    break;
  case 1:
    sprintf(s, "PLAY %c", repeat ? '#' : ' ');
    break;
  case 2:
    sprintf(s, "PAUSE%c", repeat ? '#' : ' ');
    break;
  case 3:
    sprintf(s, "STOP %c", repeat ? '#' : ' ');
    break;
  case 4:
    sprintf(s, "EOF  %c", repeat ? '#' : ' ');
    break;
  default:
    sprintf(s, "??????");
    break;
  }
  return s;
}

/*Change virtual linux console */
/*ripped from `strace chvt n` */
void 
chvt(int vt)
{
  int  ctfd;

  ctfd = open("/dev/tty", O_RDONLY);
  if (!ctfd)
    my_error("chvt","/dev/tty", errno);
  ioctl(ctfd, VT_ACTIVATE, vt);
  ioctl(ctfd, VT_WAITACTIVE, vt);
  close(ctfd);
}

/*Clear path before songname */
int 
clp(char *s)
{
  int  i;

  for (i = strlen(s); i > 0 && s[i] != '/'; i--);
  return i + 1;
}


/*Edit playlist. 
 *Scroll through playlist and remove entries*/
int 
pledit()
{
  int  i, j, r, c;
  int  idx, pos, off = 0, larger;

  char buffer[256], key;
  char *p;

  idx = 0;
  pos = 0;
  off = 0;
  larger = 0;

#ifdef LCD
  LcdClear();
#else

#endif
  memset(lcd_bp, ' ', 80);

  for (;;) {

    for (i = 0; i < 4 && idx + i < plsi; i++) {		/* row by row */
      p = playlist[idx + i];
      if ((j = strlen(p)) > larger)
	larger = j;
      if (off < clp(p))
	off = clp(p);		/* Don't show the path */
      sprintf(buffer, "%c%-19s", i == pos ? 126 : ' ',
	      strlen(p) <= off ? " " : p + off);
      strncpy(&lcd_b[i][0], buffer, 20);
    }

    if (plsi)
      for (j = i; j < 4; j++) {
	sprintf(buffer, "                     ");
	strncpy(&lcd_b[j][0], buffer, 20);
      }
    else {
      strncpy(&lcd_b[i][0], " PLAYLIST   EMPTY!  ", 20);
      for (j = i + 1; j < 4; j++) {
	sprintf(buffer, "                     ");
	strncpy(&lcd_b[j][0], buffer, 20);
      }
    }

#ifdef LCD

    LcdUpdate(lcd_bp, lcd_b);
#else
    WriteScreen(lcd_b);
#endif
    for (c = 0; c < 20; c++)
      for (r = 0; r < 4; r++)
	lcd_bp[r][c] = lcd_b[r][c];


    //r = read(fdc, buffer, 256);
    //key = *buffer;
    key=readinput();

    if (key == K_SCDOWN) {	/* down */
      if ((pos < 3) && (idx + pos < plsi - 1))
	pos++;
      else if (pos + idx < plsi - 1) {
	pos = 0;
	idx += 4;
      }
    }
    else if (key == K_SCUP) {	/* up */
      if (pos > 0)
	pos--;
      else if (idx != 0) {
	pos = 3;
	idx -= 4;
      }
    }
    else if (key == K_SCTOP) {/* first entry */
      pos = 0;
      idx = 0;
    }
    else if (key == K_SCBOTTOM) {/* last entry */
      if (plsi>4) {
	pos = 0;
	idx = plsi-plsi%4;
      }
    }
    else if (key == K_PLEDIT_SAVE && plsi) {	/* Save playlist */
      FILE *pfp;
      DIR *dirp;
      struct dirent *dp;

      dirp = opendir(pldir);
      if (!dirp)
	my_error("pledit",pldir, errno);
      j = 0;
      while ((dp = readdir(dirp)) != NULL) {
	j++;			/* number of files in pldir */
      }
      j -= 2;			/* substract . and .. */

      sprintf(buffer, "%s/playlist-%02d", pldir, j);
      /* pfp=stdout */
      if ((pfp = fopen(buffer, "w")) == NULL) {
	my_error("pledit",buffer, errno);
      }
      for (i = 0; i < plsi; i++) {
	fprintf(pfp, "%s\n", playlist[i]);
      }
      fclose(pfp);
      strncpy(&lcd_t[0][0], "********************", 20);
      strncpy(&lcd_t[1][0], "*       SAVED      *", 20);
      sprintf(buffer,      "*    playlist-%02d   *", j);
      strncpy(&lcd_t[2][0], buffer, 20);
      strncpy(&lcd_t[3][0], "********************", 20);
#ifdef LCD
      LcdUpdate(lcd_b, lcd_t);
      sleep(2);
      LcdUpdate(lcd_t, lcd_b);
#else
      WriteScreen(lcd_t);
      sleep(2);
      WriteScreen(lcd_b);
#endif
    }
    else if (key == K_SCRIGHT && off < larger - 15) {	/* right */
      off++;
    }
    else if (key == K_SCLEFT && off > 0) {	/* left */
      off--;
    }
    else if (key == K_PLEDIT_CLEAR) {	/* clear playlist */
      for (i = 0; i < plsi; i++)
	if (playlist[i]) {
	  free(playlist[i]);
	  playlist[i] = NULL;	/* for realloc */
	}
      plsi = 0;
    }
    else if (key == K_PLEDIT_RANDOMIZE) {   /* select randomly */
      unsigned int next=0;
      char *p;

      /* Initialize a random sequence with a time dependent seed */
      srand((unsigned int) time((time_t *) 0));
      /* Select one each time,
	 reduce the selection field, 
	 select one from the rest */
      for (i = 0; i < plsi; i++) {
	if (plsi - 1 - i)
	  next = i + rand() % (plsi - 1 - i);
	if (i != next) {
	  p = playlist[i];
	  playlist[i] = playlist[next];
	  playlist[next] = p;
	}
      }
    }
    else if (key == K_PLEDIT_REMOVE && plsi) {	/* remove current from
						 * playlist */
#ifdef DEBUG1
      printf("\nRemoving %s from playlist\n", playlist[idx + pos]);
#endif
      free(playlist[idx + pos]);
      playlist[idx + pos] = NULL;	/* for realloc */
      for (j = idx + pos; j < plsi - 1; j++)
	playlist[j] = playlist[j + 1];
      plsi--;
      playlist[plsi] = NULL;	/* for realloc */
      if (pos + idx > plsi - 1 && plsi > 0) {
	idx = ((plsi - 1) / 4) * 4;
	pos = plsi - 1 - idx;
      }
    }
    else if (key == K_PLEDIT_GOBACK) {	/* go back */
#ifdef DEBUG1
      for (i = 0; i < plsi; i++)
	printf("PPlaylist [%d]:(%s)\n", i, playlist[i]);
#endif
      return plsi;
    }
  }
}

int 
menusel()
{
  int  i = 0, j = 0;
  int  idx, pos;
  unsigned int off = 0;
  int  r, c;
  int  menusize = 0;
  int  larger = 0;
  char *p;
  static char *menus[16];
  char buffer[256], key;

  idx = 0;
  pos = 0;


#ifdef LCD
  LcdClear();
#endif
  memset(lcd_bp, ' ', 80);

  menus[0] = (char *) realloc(menus[0], 21);
  menus[1] = (char *) realloc(menus[1], 21);
  menus[2] = (char *) realloc(menus[2], 21);
  menus[3] = (char *) realloc(menus[3], 21);
  strncpy(menus[0], "File Selection     ", 20);
  strncpy(menus[1], "Playlist Edit      ", 20);
  strncpy(menus[2], "Browse Playlists   ", 20);
  strncpy(menus[3], "Shutdown MP3OPHONO ", 20);

  menusize = 4;
  larger = 20;


  for (;;) {

    for (i = 0; i < 4 && (idx + i) < menusize; i++) {	/* row by row */
      sprintf(buffer, "%c%-19s", i == pos ? 126 : ' ',
	      strlen(p = menus[idx + i]) > off ? p + off : " ");
      strncpy(&lcd_b[i][0], buffer, 20);
    }

    for (j = i; j < 4; j++) {
      sprintf(buffer, "                     ");
      strncpy(&lcd_b[j][0], buffer, 20);
    }

#ifdef LCD
    LcdUpdate(lcd_bp, lcd_b);
#else
    WriteScreen(lcd_b);
#endif
    for (c = 0; c < 20; c++)
      for (r = 0; r < 4; r++)
	lcd_bp[r][c] = lcd_b[r][c];


    /*
    r = read(fdc, buffer, 256);
    key = *buffer;
    */
    key=readinput();

    if (key == K_SCRIGHT && off < larger - 15) {	/* scroll right */
      off++;
    }
    else if (key == K_SCLEFT && off > 0) {	/* scroll left */
      off--;
    }
    else if (key == K_MENUSEL_SEL) {	/* Select this one */
      switch (pos) {
      case 0:
	return K_FSEL;
	break;
      case 1:
	return K_PLEDIT;
	break;
      case 2:
	return K_PLBRSE;
	break;
      case 3:
	return -1;
	break;
      }
    }
    else if (key == K_SCDOWN) {	/* down */
      if (pos < 3 && idx + pos < menusize - 1)
	pos++;
      else if (pos + idx < menusize - 1) {
	pos = 0;
	idx += 4;
      }
    }
    else if (key == K_SCUP) {	/* up */
      if (pos > 0)
	pos--;
      else if (idx != 0) {
	pos = 3;
	idx -= 4;
      }
    }
    else if (key == K_MENUSEL_UPGRD) {		/* upgrade binary */
      memset(lcd_bp, ' ', 80);
      strncpy(&lcd_bp[0][0], "  Floppy  Software  ", 20);
      strncpy(&lcd_bp[1][0], "      Upgrade       ", 20);
      strncpy(&lcd_bp[2][0], "Hit Again to Confirm", 20);
#ifdef LCD
      LcdClear();
      LcdWrite(lcd_bp);
#else
      WriteScreen(lcd_bp);
#endif
      r = read(fdc, buffer, 256);
      key = *buffer;
      if (key == K_MENUSEL_UPGRD){
	/*This code really sucks*/
	sprintf(buffer,"%s -n %s %s",mcopy,rxsrc,rxdst);
	system(buffer);
	exit(-12);
      }
    }
    else if (key == K_MENUSEL_GOBACK) {		/* go back */
      return 0;
    }
  }

}

/*Load a playlist from disk */
int 
plbrowse()
{
  struct dirent *dp;
  DIR *dirp;
  char dir[1024];
  char buffer[256], key;
  int  i = 0, j = 0, chd = 1, r, c;
  unsigned int plsize=0, idx, pos, off = 0;
  struct stat stp;
  char refdir[512];		/* reference path  *(for the fullpath) */
  char *p;
  int  filter = 1, larger=0;


  idx = 0;
  pos = 0;
  strcpy(refdir, pldir);


#ifdef LCD
  LcdClear();
#endif
  memset(lcd_bp, ' ', 80);

  for (;;) {
    if (chd) {
      dirp = opendir(refdir);
      if (!dirp)
	my_error("plbrowse",refdir, errno);
      i = 0;
      while ((dp = readdir(dirp)) != NULL) {
	if (filter) {		/* don't display "."&".." */
	  strcpy(dir, refdir);
	  strcat(dir, dp->d_name);
	  stat(dir, &stp);
	  if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
	    plfiles[i] = (char *) realloc(plfiles[i], strlen(dp->d_name) + 2);
	    if ((j = strlen(dp->d_name)) > larger)
	      larger = j;
	    strcpy(plfiles[i], dp->d_name);
	    i++;
	    /* printf("plbrowse:%d %s\n",i,dp->d_name); */
	  }
	}
      }
      plsize = i;		/* size of plfiles[] i.e. #of  *plfiles in
				 * dir 
				 * points to the next free  *entry */
      closedir(dirp);

#ifdef SORT
      printf("BEFORE, plsize=%d\n",plsize);
      qsort1(0, plsize-1, plfiles);	/* sort alphabetically */
      printf("After QSORT\n");
#endif

      chd = 0;
      pos = 0;
      idx = 0;
    }

    if (plsize) {
      for (i = 0; i < 4 && (idx + i) < plsize; i++) {	/* row by row */
	sprintf(buffer, "%c%-19s", i == pos ? 126 : ' ',
		strlen(p = plfiles[idx + i]) > off ? p + off : " ");
	strncpy(&lcd_b[i][0], buffer, 20);
      }

      for (j = i; j < 4; j++) {
	sprintf(buffer, "                     ");
	strncpy(&lcd_b[j][0], buffer, 20);
      }
    }
    else {
      strncpy(&lcd_b[0][0], " DIRECTORY  EMPTY! ", 20);
      for (j = 1; j < 4; j++) {
	sprintf(buffer, "                     ");
	strncpy(&lcd_b[j][0], buffer, 20);
      }
    }

#ifdef LCD
    LcdUpdate(lcd_bp, lcd_b);
#else
    WriteScreen(lcd_b);
#endif
    for (c = 0; c < 20; c++)
      for (r = 0; r < 4; r++)
	lcd_bp[r][c] = lcd_b[r][c];


    //r = read(fdc, buffer, 256);
    //key = *buffer;
    key=readinput();

    if (key == K_PLBRSE_GOBACK || !plsize) {	/* go back */
#ifdef DEBUG1
      for (i = 0; i < plsi; i++)
	printf("PPplaylist [%d]:(%s)\n", i, playlist[i]);
#endif
      return plsi;
    }
    else if (key == 8 || key == 127) {
      chd = 1;
      strcat(refdir, "/../");
    }
    else if (isdigit(c)) {
      strcat(refdir, "/");
      strcat(refdir, plfiles[c - '0']);
      strcat(refdir, "/");
    }
    else if (key == K_SCRIGHT && off < larger - 15 - strlen(refdir)) {	
      /* scroll right */
      off++;
    }
    else if (key == K_SCLEFT && off > 0) {	/* scroll left */
      off--;
    }
    else if (key == K_PLBRSE_DEL && plsize) {	/* Delete this one */
      strcpy(dir, refdir);
      strcat(dir, "/");
      strcat(dir, plfiles[idx + pos]);
      stat(dir, &stp);
      if (!S_ISDIR(stp.st_mode)) {
#ifdef DEBUG1
	printf("\nunlinking:%s\n\n", dir);
#endif
	unlink(dir);		/* Delete the file */
	free(plfiles[idx + pos]);
	plfiles[idx + pos] = NULL;
	for (j = idx + pos; j < plsize - 1; j++)
	  plfiles[j] = plfiles[j + 1];
	plsize--;
	plfiles[plsize] = NULL;	/* for realloc */
	if (pos + idx > plsize - 1 && plsize > 0) {
	  idx = ((plsize - 1) / 4) * 4;
	  pos = plsize - 1 - idx;
	}

      }
    }
    else if (key == K_PLBRSE_SEL) {	/* Select this one */
      strcpy(dir, refdir);
      strcat(dir, plfiles[idx + pos]);
      stat(dir, &stp);
      if (S_ISDIR(stp.st_mode)) {
	strcat(refdir, "/");
	strcat(refdir, plfiles[idx + pos]);
	strcat(refdir, "/");
	chd = 1;

#ifdef DEBUG1
	printf("\nDIR:%s\n\n", refdir);
#endif
      }
      else {			/* If it's a plfile */
	FILE *pfp;
	char item[256];

	plsi = 0;
	strcpy(dir, refdir);
	strcat(dir, "/");
	strcat(dir, plfiles[idx + pos]);

	if ((pfp = fopen(dir, "r")) == NULL) {
	  my_warn("plbrowse",dir, errno);
	  return -1;
	}
	plsi = 0;
	while (fgets(item, 256, pfp)) {
	  item[strlen(item) - 1] = '\0';

	  if ((r = stat(item, &stp)) == -1) {
	    my_warn("plbrowse",item, errno);
	  }
	  else if (!S_ISDIR(stp.st_mode) && strstr(item, ".mp")) {
	    playlist[plsi] = realloc(playlist[plsi], strlen(item) + 2);
	    strcpy(playlist[plsi], item);
#ifdef DEBUG1
	    printf("pll added:#%s#\n", playlist[plsi]);
#endif
	    plsi++;
	  }
	}
	fclose(pfp);
#ifdef DEBUG1
	printf("TOTAL pll added:%d files\n", plsi);
#endif
	return plsi;
      }				/* it's a playlist */
    }				/* \n */
    else if (key == K_SCDOWN) {	/* down */
      if (pos < 3 && idx + pos < plsize - 1)
	pos++;
      else if (pos + idx < plsize - 1) {
	pos = 0;
	idx += 4;
      }
    }
    else if (key == K_SCUP) {	/* up */
      if (pos > 0)
	pos--;
      else if (idx != 0) {
	pos = 3;
	idx -= 4;
      }
    }
  }

}


/*(File Select) Add entries to the playlist */
int 
filesel()
{

  struct dirent *dp;
  DIR *dirp;
  char dir[1024];
  char buffer[256], key;
  int  i = 0, j = 0, chd = 1, r, c;
  int  size=0,idx, pos, off = 0, tsel = 0;
  struct stat stp;
  char refdir[512];		/* reference path  (we need fullpath) */
  char *p;
  int  filter = 1, larger;


  idx = 0;
  tsel = 0;			/* # of times it has been selected */
  pos = 0;
  larger = 0;
 
  if (startdir[strlen(startdir)-1]!='/')
    strcat(startdir,"/");

  if (chdir(startdir)!=0) my_error("filesel",startdir,errno);
  strcpy(refdir, startdir);


#ifdef LCD
  LcdClear();
#endif
  memset(lcd_bp, ' ', 80);

  for (;;) {
    if (chd) {
      dirp = opendir(refdir);
      if (!dirp)
	my_error("filesel",refdir, errno);
      i = 0;
      while ((dp = readdir(dirp)) != NULL) {
	if (filter) {		/* Display only dirs and .mp? files */
	  strcpy(dir, refdir);
	  strcat(dir, dp->d_name);
	  stat(dir, &stp);
	  if (S_ISDIR(stp.st_mode) || strstr(dp->d_name, ".mp") ||
	      !strcmp(dp->d_name, "..")) {
	    files[i] = (char *) realloc(files[i], strlen(dp->d_name) + 2);
	    if ((j = strlen(dp->d_name)) > larger)
	      larger = j;
	    strcpy(files[i], dp->d_name);
	    i++;
	    /* printf("%d %s\n",i,dp->d_name); */
	  }
	}
      }
      size = i;		/* size of files[] i.e.#of files in dir */
			/* points to the next free entry */
      closedir(dirp);

#ifdef SORT
      qsort1(2, size - 1, files);	/* sort alphabetically */
#endif


      chd = 0;
      pos = 0;
      idx = 0;
    }

    for (i = 0; i < 4 && (idx + i) < size; i++) {	/* one loop for each
							 * row */
      tsel = 0;
      for (j = 0; j < plsi; j++) {
	/* 
	 * if (strlen(files[idx+i])>3 && strstr(playlist[j],files[idx+i]))  */
	if (strlen(files[idx + i]) > 3 &&
	    strstr(playlist[j], files[idx + i]) &&
	    strstr(files[idx + i], ".mp")) {
	  tsel++;
#ifdef DEBUG2
	  printf("\n%s SAME AS %s\n", playlist[j], files[idx + i]);
#endif
	}
      }
      /* Display how many times this song exists in the playlist */
      if (tsel == 0)
	tsel = ' ';		/* none */
      else if (tsel == 1)
	tsel = '*';		/* one */
      else
	tsel += '0';		/* more */
      sprintf(buffer, "%c%-19s", i == pos ? 126 : tsel,
	      strlen(p = files[idx + i]) > off ? p + off : " ");
      strncpy(&lcd_b[i][0], buffer, 20);
    }


    for (j = i; j < 4; j++) {
      memset(&lcd_b[j][0], ' ', 20);
    }
#ifdef LCD
    LcdUpdate(lcd_bp, lcd_b);
#else
    WriteScreen(lcd_b);
#endif
    for (c = 0; c < 20; c++)
      for (r = 0; r < 4; r++)
	lcd_bp[r][c] = lcd_b[r][c];


    //r = read(fdc, buffer, 256);
    //key = *buffer;
    key=readinput();

    if (key == 8 || key == 127) {
      chd = 1;
      strcat(refdir, "/../");
    }
    else if (isdigit(c)) {
      strcat(refdir, "/");
      strcat(refdir, files[c - '0']);
      strcat(refdir, "/");
    }
    else if (key == K_SCRIGHT && off < larger - 15 - strlen(refdir)) {	
						/* scroll right */
      off++;
    }
    else if (key == K_SCLEFT && off > 0) {	/* scroll left */
      off--;
    }
    else if (key == K_FSEL_SEL) {
      strcpy(dir, refdir);
      strcat(dir, files[idx + pos]);
      stat(dir, &stp);
      if (S_ISDIR(stp.st_mode)) {
	strcat(refdir, "/");
	strcat(refdir, files[idx + pos]);
	strcat(refdir, "/");
	chd = 1;

#ifdef DEBUG1
	printf("\nDIR:%s\n\n", refdir);
#endif
      }
      else if (strstr(files[idx + pos], ".mp")) {	/* If it's a file */
#ifdef DEBUG1
	printf("%s:IS An MP3\n", files[idx + pos]);
#endif
	playlist[plsi] = realloc(playlist[plsi],
			     strlen(refdir) + strlen(files[idx + pos]) + 2);
	strcpy(playlist[plsi], refdir);
	strcat(playlist[plsi], files[idx + pos]);

	realpath(playlist[plsi], buffer);
	playlist[plsi] = realloc(playlist[plsi], strlen(buffer));
	strcpy(playlist[plsi], buffer);

	plsi++;
      }
      else
	printf("ISNTDIR(%s)\n\n", dir);
    }
    else if (key == K_FSEL_RANDSEL) {	/* select randomly */
      unsigned int next=0;
      char *p;

      /* Initialize a random sequence with a 
	 time dependent seed */
      srand((unsigned int) time((time_t *) 0));
      /* Select one each time,
	 then reduce the selection field, 
	 and select one from the rest */
      for (i = 2; i < size; i++) {
	if (size - 1 - i)
	  next = i + rand() % (size - 1 - i);
	if (i != next) {
	  p = files[i];
	  files[i] = files[next];
	  files[next] = p;
	}
      }

#ifdef DEBUG1
      for (i = 0; i < size; i++)
	printf("Files[%d]:%s\n", i, files[i]);
      printf("\n");
#endif
      for (i = 0; i < size; i++) {
	strcpy(dir, refdir);
	strcat(dir, files[i]);
	stat(dir, &stp);
	if (!S_ISDIR(stp.st_mode) && strstr(files[i], ".mp")) {
	  playlist[plsi] = realloc(playlist[plsi], strlen(dir)+ 2);
	  strcpy(playlist[plsi],dir);
	  plsi++;
#ifdef DEBUG1
	  printf("K_FSEL_RANDSEL:added[%d]:%s\n", i, files[i]);
#endif
	}
      }

    }
    else if (key == K_FSEL_SELALL) {	/* select all */

      for (i = 0; i < size; i++) {
	strcpy(dir, refdir);
	strcat(dir, files[i]);
	stat(dir, &stp);
        /* If it's an mpx file */
	if (!S_ISDIR(stp.st_mode) && strstr(files[i], ".mp")) {	
	  playlist[plsi] = realloc(playlist[plsi], strlen(dir)+ 2);
	  strcpy(playlist[plsi],dir);
#ifdef DEBUG1
	  printf("K_FSEL_SELALL:added:%s\n", dir);
#endif
	  plsi++;
	}
      }
    }
    else if (key == K_FSEL_CLEARPL) {	/* clear playlist */
#ifdef DEBUG1
      printf("plsi:%d\n", plsi);
#endif
      for (i = 0; i < plsi; i++)
	if (playlist[i]) {
	  free(playlist[i]);
	  playlist[i] = NULL;	/* Needed by realloc */
	}
      plsi = 0;
      strncpy(&lcd_t[0][0], "********************", 20);
      strncpy(&lcd_t[1][0], "*     Playlist     *", 20);
      strncpy(&lcd_t[2][0], "*      Erased      *", 20);
      strncpy(&lcd_t[3][0], "********************", 20);
#ifdef LCD
      LcdUpdate(lcd_b, lcd_t);
      usleep(800000);
      LcdUpdate(lcd_t, lcd_b);
#else
      WriteScreen(lcd_t);
      usleep(800000);
      WriteScreen(lcd_b);
#endif
    }
    else if (key == K_FSEL_RECSEL) {	/* select all files recursively */
      strcpy(dir, refdir);
      strcat(dir, files[idx + pos]);
      recursive_select(20,dir);
    }
    else if (key == K_SCDOWN) {	/* down */
      if ((pos < 3) && (idx + pos < size - 1))
	pos++;
      else if (pos + idx < size - 1) {
	pos = 0;
	idx += 4;
      }
    }
    else if (key == K_SCTOP) {/* first entry */
      pos = 0;
      idx = 0;
    }
    else if (key == K_SCBOTTOM) {/* last entry */
      if (size>4) {
	pos = 0;
	idx = size-size%4;
      }
    }
    else if (key == K_SCUP) {	/* up */
      if (pos > 0)
	pos--;
      else if (idx != 0) {
	pos = 3;
	idx -= 4;
      }
    }
    else if (key == K_PLEDIT_GOBACK) {	/* go back */
#ifdef DEBUG1
      for (i = 0; i < plsi; i++)
	printf("FPlaylist [%d]:(%s)\n", i, playlist[i]);
#endif
      return plsi;
    }
  }

}


int
recursive_select(int depth,char * refdir)
{
  int i,size=0;
  char dir[1024];
  struct stat stp;
  DIR *dirp;
  struct dirent *dp;

  depth--;


  if (depth<0) {
      printf("recursive_select: maximum depth reached, aborting\n");
      return -1;
  }
  dirp = opendir(refdir);
  if (!dirp) {
    my_warn("recursive_select",refdir, errno);
    printf("recursive_select(%d):ignoring:%s\n",depth,refdir); 
    return -1;
  }
  i = 0;
  while ((dp = readdir(dirp)) != NULL) {
    strcpy(dir, refdir);
    strcat(dir, "/");
    strcat(dir, dp->d_name);
    stat(dir, &stp);
    if (S_ISDIR(stp.st_mode) && 
	strcmp(dp->d_name,".") && strcmp(dp->d_name,"..")) {
      recursive_select(depth,dir);
#ifdef DEBUG1
      printf("recursive_select(%d):descending:%s\n",depth,dp->d_name); 
      fflush(stdout);
#endif
    }
    else if (strstr(dp->d_name,".mp")) {
      playlist[plsi] = realloc(playlist[plsi], strlen(dir)+ 2);
      strcpy(playlist[plsi],dir);
      plsi++;
#ifdef DEBUG1
      printf("recursive_select(%d):added:%s\n",depth,dir);
      fflush(stdout);
#endif
    }
  }
  closedir(dirp);

#ifdef SORT
  qsort1(2, size - 1, files);       /* sort alphabetically */
#endif
  return depth;
}

    
void 
diehdl(int dummy)
{
  int  status = 0;

  while (waitpid(childpid, &status, WNOHANG)>0)
#ifdef DEBUG1
    fprintf(stderr, "\nInside childhandler,child %d died\n",childpid);
#endif
    ; exit(-13);
}


/*wait writedelay microseconds after each line written. It helps somepeople
 * with the (buggy) rxaudio, but it's the same to me even if writedelay=0.
 * If set too high, it may not work correctly (the pipe's buffer will 
 * overfow before a read() occur)
 */
int 
slowrite(int fd, char *buf, int count)
{
  int  r = 0;

  while (count--) {
    if (write(fd, buf, 1))
      r++;
    if (*buf == '\n')
      usleep(writedelay);
    buf++;
  }
  return r;
}

/* Shutdown MP3-O-PHONO */
int 
rx_shutdown()
{
  int  r;
  char key;
  char buffer[16];

  memset(lcd_bp, ' ', 80);
  strncpy(&lcd_bp[0][0], "  SHUT DOWN DEVICE  ", 20);
  strncpy(&lcd_bp[1][0], "Hit Again to Confirm", 20);
#ifdef LCD
  LcdClear();
  LcdWrite(lcd_bp);
#else
  WriteScreen(lcd_bp);
#endif
  r = read(fdc, buffer, 16);
  key = *buffer;
  if (key != K_MENUSEL_SEL)
    return 0;

#ifdef LCD
  LcdClear();
  memset(lcd_bp, ' ', 80);
  strncpy(&lcd_bp[1][0], " Please     Wait!  ", 20);
  LcdWrite(lcd_bp);
#endif

#ifdef USE_SYSLOG
  closelog();
#endif

  r = system(halt_cmd);
  slowrite(pfd[1], "exit\n", 5); /* gets here if halt fails */
  return -2;
}

void
usage()
{
  fprintf(stderr,"rxcontrol %s\n\n",version);
  fprintf(stderr,"usage rxcontrol [-f conf_file] [-p playlist] [-h]\n");
  fprintf(stderr,"\t-f: conf_file to use instead of rxcontrol.conf\n");
  fprintf(stderr,"\t-p: playlist file to load at startup\n");
  fprintf(stderr,"\t-h: display this help\n\n");
  fprintf(stderr,"Visit http://www.softlab.ece.ntua.gr/~sivann/rxcontrol\n");
  exit(0);
}

int
disable_info(int fd){
  char tmp[8];
  sprintf(tmp,"%c",16);
  return slowrite(fd, tmp, strlen(tmp));
}

int
enable_info(int fd){
  char tmp[8];
  sprintf(tmp,"%c",24);
  return slowrite(fd, tmp, strlen(tmp));
}

int 
main(int argc,char **argv)
{
  FILE * plfp;
  FILE * confp=0;
  int  r, i, j, c;
  int  track = 0;
  char buffer[512],cmd[256], response[2048], song[512], artist[64], sname[64];
  char chr,config[128],plist_file[128];
  static unsigned char scroll = 0, scrolla = 0;
  static unsigned char scrd = 0, scrda = 0;
  struct timeval tval;
  fd_set inout_fds;
  
  config[0]=plist_file[0]=0;

 
  while ((chr=getopt (argc, argv, "hf:p:")) != -1) {
    switch (chr) {
      case 'h':
	usage();
	break;
      case 'f':
	strncpy(config,optarg,127);
	break;
      case 'p':
	strncpy(plist_file,optarg,127);
	break;
    }
  }

  fprintf(stderr,"rxcontrol %s (c) Spiros Ioannou 1998-1999\n",version);
  

  if (strlen(config) && ((confp = fopen(config, "r")) == NULL)) {
    fprintf(stderr,"%s: %s, aborting\n",config,strerror(errno));
    exit(-5);
  }
  else if (!strlen(config) && ((confp = fopen("rxcontrol.conf", "r")) == 0)) {
    fprintf(stderr,"rxcontrol.conf: %s, aborting\n",strerror(errno));
    exit(-6);
  }

  printf("\n*** reading configuration file: %s\n",config);
  cs.error=0;

  /*Read string options*/
  strcpy(cs.soption,"port");cs.type=0; readconf(confp,&cs);
  strcpy(port,cs.svalue);
  printf("port:        %s\n",port);

  strcpy(cs.soption,"startdir");cs.type=0; readconf(confp,&cs);
  strcpy(startdir,cs.svalue);
  printf("startdir:    %s\n",startdir);

  strcpy(cs.soption,"pldir");cs.type=0; readconf(confp,&cs);
  strcpy(pldir,cs.svalue);
  printf("pldir:       %s\n",pldir);

  strcpy(cs.soption,"rxpath");cs.type=0; readconf(confp,&cs);
  strcpy(rxpath,cs.svalue);
  printf("rxpath:      %s\n",rxpath);

  strcpy(cs.soption,"vt");cs.type=0; readconf(confp,&cs);
  strcpy(vt,cs.svalue);
  printf("vt:          %s\n",vt);

  strcpy(cs.soption,"halt_cmd");cs.type=0; readconf(confp,&cs);
  strcpy(halt_cmd,cs.svalue);
  printf("halt_cmd:    %s\n",halt_cmd);

  strcpy(cs.soption,"intro");cs.type=0; readconf(confp,&cs);
  strcpy(intro,cs.svalue);
  printf("intro:       %s\n",intro);

  strcpy(cs.soption,"rxsrc");cs.type=0; readconf(confp,&cs);
  strcpy(rxsrc,cs.svalue);
  printf("rxsrc:       %s\n",rxsrc);

  strcpy(cs.soption,"rxdst");cs.type=0; readconf(confp,&cs);
  strcpy(rxdst,cs.svalue);
  printf("rxdst:       %s\n",rxdst);

  strcpy(cs.soption,"mcopy");cs.type=0; readconf(confp,&cs);
  strcpy(mcopy,cs.svalue);
  printf("mcopy:       %s\n",mcopy);

  strcpy(cs.soption,"cdrom");cs.type=0; readconf(confp,&cs);
  strcpy(cdrom,cs.svalue);
  printf("cdrom:       %s\n",cdrom);

  strcpy(cs.soption,"cdromdir");cs.type=0; readconf(confp,&cs);
  strcpy(cdromdir,cs.svalue);
  printf("cdromdir:    %s\n",cdromdir);


  /*Read integer options*/
  strcpy(cs.soption,"contrast");cs.type=1;readconf(confp,&cs);
  contrast=cs.value;
  printf("contrast:    %d\n",contrast);

  strcpy(cs.soption,"scrolldelay");cs.type=1;readconf(confp,&cs);
  scrolldelay=cs.value;
  printf("scrolldelay: %d\n",scrolldelay);

  strcpy(cs.soption,"keydelay");cs.type=1;readconf(confp,&cs);
  keydelay=cs.value;
  printf("keydelay:    %d\n",keydelay);

  strcpy(cs.soption,"keyrate");cs.type=1;readconf(confp,&cs);
  keyrate=cs.value;
  printf("keyrate:     %d\n",keyrate);

  strcpy(cs.soption,"seekoffset");cs.type=1;readconf(confp,&cs);
  seekoffset=cs.value;
  printf("seekoffset:  %d\n",seekoffset);

  strcpy(cs.soption,"scrollstep");cs.type=1;readconf(confp,&cs);
  scrollstep=cs.value;
  printf("scrollstep:  %d\n",scrollstep);

  strcpy(cs.soption,"writedelay");cs.type=1;readconf(confp,&cs);
  writedelay=cs.value;
  printf("writedelay:  %d\n",writedelay);

  strcpy(cs.soption,"irperceive");cs.type=1;readconf(confp,&cs);
  irperceive=cs.value;
  printf("irperceive:  %d\n",irperceive);

  printf("*** Done ***\n\n");
  
  if (cs.error) {
    fprintf(stderr,"%d error(s) found, aborting..\n",cs.error);
    exit(-7);
  }

  
  /* open the command-line specified playlist */
  if (strlen(plist_file) && ((plfp = fopen(plist_file, "r")) == 0)) {
    fprintf(stderr,"%s: %s, aborting\n",plist_file,strerror(errno));
    exit(-8);
  }
  else if (strlen(plist_file)) {                    
    char item[256];
    struct stat stp;

    printf("\n*** reading playlist from: %s\n",plist_file);
    plsi = 0;
    track=0;
    while (fgets(item, 256, plfp)) {
      item[strlen(item) - 1] = '\0';
      stat(item, &stp);

      if ((r = stat(item, &stp)) == -1) {
	my_warn("main",item, errno);
      }
      else if (!S_ISDIR(stp.st_mode) && strstr(item, ".mp")) {
	playlist[plsi] = realloc(playlist[plsi], strlen(item) + 2);
	strcpy(playlist[plsi], item);
#ifdef DEBUG1
	printf("pll added:#%s#\n", playlist[plsi]);
#endif
	plsi++;
      }
    }
    fclose(plfp);
#ifdef DEBUG1
    printf("TOTAL pll added:%d files\n", plsi);
#endif
  }                         

  

#ifdef linux
  /*set the keyboard rate/delay*/
  kbrate(keyrate,keydelay);
#endif

  signal(SIGCHLD, diehdl);	/* for rxaudio */

#ifdef CHVT
  /* Open virtual console VT for i/o */
  fdc = open(vt, O_RDWR);
  if (!fdc)
    my_error("main",vt, errno);
#else
  fdc = open(ttyname(0), O_RDWR);
  if (!fdc)
    my_error("main","open ttyname(0)", errno);
#endif

  if (fdc < 0) {
    perror("open");
    exit(-1);
  }

  /* Pipes are unidirectional, we need 2 of them to communicate with rxaudio */

  pipe(pfd);			/* write to  exec'ed process (rxaudio) */
  pipe(pfd2);			/* read from exec'ed process (rxaudio) */

  childpid = fork();

  if (childpid == 0) {
    char path[256];
    char *av[3];

    /* close unused side of pipes */
    close(pfd[1]);
    close(pfd2[0]);

    /* Close up standard input of the child, */
    /* duplicate the input side of pipe to stdin */
    /* then,close up standard output of the child */
    /* duplicate the output side of pipe2 to stdout */
    /* (could use dup2) */

    close(0);
    dup(pfd[0]);		/* read from here */
    close(1);
    dup(pfd2[1]);		/* write here */

    /* rxaudio in path */
    strcpy(path, rxpath);
    strcat(path, "/rxaudio");
    av[0]="pipefilter";
    av[1]=path;
    av[2]=0;
    r = execlp("./pipefilter","pipefilter",path,0);

    if (r == -1)
      my_error("main","execlp", errno);
  }
  else {
    /* We write to pfd[1] and read from pfd2[0] */
    unsigned int volume = 60;
    unsigned int ack = 0, level = 0, layer = 0, duration = 0, bitrate = 0,
         frequency = 0;
    unsigned int tot_hours = 0, tot_minutes = 0, tot_seconds = 0;
    unsigned int rem_minutes = 0, rem_seconds = 0, elapsed = 0;
    int  ready = 0, offset = 0, range = 0, hours = 0, minutes = 0, seconds = 0;
    char state = 0, pstate = 0, newstate = 0;
    char type[64], tmp[128];
    char mode[32];
    int irinput=0;


    /* close unused side of pipes */
    close(pfd[0]);
    close(pfd2[1]);

#ifdef USE_SYSLOG
    openlog("rxcontrol", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);
    /* syslog(LOG_USER, "STARTING");*/
#endif
    /* Does this work for pipes? the manual doesn't say */
    fcntl(pfd2[0], F_SETFL, O_NONBLOCK);

    strcpy(buffer, "Daemon ready\n");
    write(fdc, buffer, strlen(buffer));

#ifdef CHVT
    chvt(5);
#endif
    /* Accept keystrokes at once */
    no_wait(fdc);

#ifdef HAVEIR
    irsd=ir_init();
    ir_loadkeys();
#endif

#ifdef LCD
    /* Initialize lcd */

    if ((lfp = fopen(port, "w")) == 0)
      my_error("main",port, errno);

    setvbuf(lfp, (char *) 0, _IONBF, 0);	/* Unbuffered output to LCD */

    /* Set Line Parmeters */
    termios_p = (struct termios *) malloc(sizeof(struct termios));

    tcgetattr(fileno(lfp), termios_p);
    cfsetospeed(termios_p, B19200);
    cfmakeraw(termios_p);	/* No translations! */
    tcsetattr(fileno(lfp), TCSADRAIN, termios_p);

    /* Configure LCD */
    fprintf(lfp, "%cC", prefix);	/* Wrapping On */
    fprintf(lfp, "%cR", prefix);	/* Scroll Off */
    fprintf(lfp, "%cK", prefix);	/* Cursor Off */
    fprintf(lfp, "%cT", prefix);	/* Blink Off */
    fprintf(lfp, "%ch", prefix);	/* init H-Bar Graph */
    fprintf(lfp, "%cP%c", prefix, contrast);	/* Contrast */

    strncpy(&lcd_bp[0][0], "********************", 20);
    strncpy(&lcd_bp[1][0], "*    Welcome to    *", 20);
    strncpy(&lcd_bp[2][0], "*   MP-3-O-Phono   *", 20);
    strncpy(&lcd_bp[3][0], "********************", 20);

    LcdClear();

    for (i = 0; i < 10; i++) {
      fprintf(lfp, "%cF", prefix);
      usleep(50000);
      fprintf(lfp, "%cB%c", prefix, 0);
      usleep(50000);
    }

    LcdWrite(lcd_bp);

    /* 
     * sleep(1); */


#endif
    do {
      FD_ZERO(&inout_fds);
      FD_SET(fdc, &inout_fds);	/* FROM keyboard */
      FD_SET(pfd[1], &inout_fds);	/* TO rxaudio */
      FD_SET(pfd2[0], &inout_fds);	/* FROM rxaudio */
#ifdef HAVEIR
      FD_SET(irsd,&inout_fds);		/* from IR */
#endif
      /* tval is needed if we need to update lcd more frequently  than the
       * data flow ( <1sec) */
      tval.tv_sec = 1;		/* Seconds */
      tval.tv_usec = 500000;	/* microseconds */

      /* No timeout (interrupt driven) select */
      i = select(16, &inout_fds, (fd_set *) 0, (fd_set *) 0, 0 /* &tval */ );


#ifdef HAVEIR
      if (FD_ISSET(irsd,&inout_fds)) {
	i = read(irsd, irdata.buf, sizeof(irdata.buf));
	ir_parsebuf(&irdata);
#ifdef DEBUG1
	printf("key:%s,serial:%d, rxkey:%c\n",
	  irdata.key,irdata.serial,irdata.rxkey);
#endif
	irinput=irdata.rxkey;
	/*accept multiple keypresses only for some keys such as FF,FB*/
	if (irdata.serial && irinput!=K_FF && irinput!=K_FB)
	  irinput=0;
	/*don't accept all pulses, ignore some to reduce rate*/
	else if (irinput==K_FF || irinput==K_FB) { 
	  if (irdata.serial%irperceive) 
	    irinput=0;
	}
#ifdef DEBUG1
	printf("irinput:%c\n",irinput);
#endif
      }
#endif

      /* Input from keyboard or IR*/
      if (FD_ISSET(fdc, &inout_fds)||irinput) {
	if (!irinput) {
	  r = read(fdc, buffer, 511);
	  buffer[r] = 0;
	}
	else 
	  *buffer=irinput;
	irinput=0;

	switch (*buffer) {

	case 't':
	  /* tray open/close */
	  tray_open = tray_open ? 0 : 1;
	  if (tray_open)
	    opentray();
	  else
	    closetray();
	  strcpy(cmd, "");
	  break;
	case K_REPEAT:
	  /* toggle repeat */
	  repeat = repeat ? 0 : 1;
	  strcpy(cmd, "");
	  break;
	case K_PLAY:
	  /* play */
	  scrolla = scroll = 0;
	  if (state == 4)	/* EOF */
	    strcpy(cmd, "stop\nplay\n");
	  else if (state == 2) {	/* pause */
	    /* pause is jerky on linux */
	    /* seek before unpause helps a little */
	    sprintf(cmd,
		    "seek %d %d\nplay\n", offset, range);
	  }
	  else if (state == 1) {	/* play */
	    /* restart sprintf(cmd,"seek %d %d\nplay\n", 0, 100);*/

	    /* pause*/  sprintf(cmd,"pause\n");
	  }

	  else if (plsi)
	    strcpy(cmd, "play\n");

	  break;
	case K_PTRACK:
	  /* previous track */
	  if (track > 0) {
	    track--;
	    j = strcmp(song, playlist[track]);
	    if (j) {
	      strcpy(song, playlist[track]);
	      sprintf(cmd, "stop\ninput_open %s\n", song);
	    }
	    else
	      strcpy(cmd, "stop\n");
	    if (state == 1)
	      strcat(cmd, "play\n");
	    tot_hours = tot_minutes = tot_seconds = 0;
	    elapsed = rem_minutes = rem_seconds = 0;
	    minutes = seconds = hours = 0;
	    id3_main(song, &id3data);
	    strncpy(artist, id3data.artist, 63);
	    strncpy(sname, id3data.songname, 63);
	    scrolla = scroll = 0;
#ifdef LCD
	    LcdClear();
#endif
	    bzero(lcd_bp, 80);
	  }
	  else
	    strcpy(cmd, "");
	  break;

	case K_NTRACK:		/* next track */
	  if (track < plsi - 1) {
	    track++;
	    /* Don't repoen the same file twice *(buggy in rxaudio4linux */
	    j = strcmp(song, playlist[track]);
	    if (j) {
	      strcpy(song, playlist[track]);
	      sprintf(cmd, "stop\ninput_open %s\n", song);
	    }
	    else
	      strcpy(cmd, "stop\n");
	    if (state == 1)
	      strcat(cmd, "play\n");
	    tot_hours = tot_minutes = tot_seconds = 0;
	    elapsed = rem_minutes = rem_seconds = 0;
	    minutes = seconds = hours = 0;
	    id3_main(song, &id3data);
	    strcpy(artist, id3data.artist);
	    strcpy(sname, id3data.songname);
	    scrolla = scroll = 0;
#ifdef LCD
	    LcdClear();
#endif
	    bzero(lcd_bp, 80);
	  }
	  else if (repeat && plsi)
	    track = 0;
	  else
	    strcpy(cmd, "");
	  break;
	case K_MENUSEL:	/* Select one of the 3 menus */
	  disable_info(pfd[1]);
	  switch (menusel()) {
	    case K_PLEDIT:
	      pledit();
	      break;
	    case K_PLBRSE:
	      plbrowse();
	      break;
	    case K_FSEL:
	      filesel();
	      break;
	    case -1:
	      rx_shutdown();
	      break;
	  }
	  enable_info(pfd[1]);
	  if (plsi > 0) {
	    if (track >= plsi)
	      track = 0;

	    /*Don't reopen the same file. Rxaudio bug*/
	    j = strcmp(song, playlist[track]);
	    if (j) {
	      strcpy(song, playlist[track]);
	      sprintf(cmd, "stop\ninput_open %s\n", song);
	    }
	   // else
	    //  strcpy(cmd, "stop\n");
	    tot_hours = tot_minutes = tot_seconds = 0;
	    elapsed = rem_minutes = rem_seconds = 0;
	    minutes = seconds = hours = 0;

	    id3_main(song, &id3data);
	    strncpy(artist, id3data.artist, 63);
	    strncpy(sname, id3data.songname, 63);
	    /* strcat(sname,id3data.comment) */
	    scrolla = scroll = 0;
#ifdef LCD
	    LcdClear();
#endif
	    bzero(lcd_bp, 80);
	  }
	  else {
	    /* allow umount */
	    strcpy(cmd, 
		"\ninput_close\n");	
	  }
	  break;

	case K_PLEDIT:		/* edit playlist in memory */
#ifdef DEBUG2
	  printf("CMD:stop\n");
#endif
	  slowrite(pfd[1], "stop\n", 5);
	  pledit();
	  /* track=0; */
	  if (plsi > 0) {
	    if (track >= plsi)
	      track = 0;
	    strcpy(song, playlist[track]);
	    sprintf(cmd, "stop\ninput_open %s\n", song);
	    tot_hours = tot_minutes = tot_seconds = 0;
	    elapsed = rem_minutes = rem_seconds = 0;
	    minutes = seconds = hours = 0;

	    id3_main(song, &id3data);
	    strncpy(artist, id3data.artist, 63);
	    strncpy(sname, id3data.songname, 63);
	    /* strcat(sname,id3data.comment) */
	    scrolla = scroll = 0;
#ifdef LCD
	    LcdClear();
#endif
	    bzero(lcd_bp, 80);
	  }
	  else {
	    strcpy(cmd, "\nclose\n");
	  }
	  break;
	case K_PLBRSE:		/* browse saved playlists in disk */
#ifdef DEBUG2
	  printf("CMD:stop\n");
#endif
	  slowrite(pfd[1], "stop\n", 5);
	  plbrowse();
	  /* track=0; */
	  if (plsi > 0) {
	    strcpy(song, playlist[track]);
	    sprintf(cmd, "stop\ninput_open %s\n", song);
	    tot_hours = tot_minutes = tot_seconds = 0;
	    elapsed = rem_minutes = rem_seconds = 0;
	    minutes = seconds = hours = 0;

	    id3_main(song, &id3data);
	    strcpy(artist, id3data.artist);
	    strcpy(sname, id3data.songname);
	    scrolla = scroll = 0;
#ifdef LCD
	    LcdClear();
#endif
	    bzero(lcd_bp, 80);
	  }
	  else {
	    strcpy(cmd, "\nclose\n");
	  }
	  break;
	case K_FSEL:
	  /* Select files from disk */
#ifdef DEBUG2
	  printf("CMD:stop\n");
#endif
	  slowrite(pfd[1], "stop\n", 5);
	  filesel();
	  if (plsi > 0) {
	    if (track >= plsi)
	      track = 0;
	    strcpy(song, playlist[track]);
	    sprintf(cmd, "stop\ninput_open %s\n", song);
	    tot_hours = tot_minutes = tot_seconds = 0;
	    elapsed = rem_minutes = rem_seconds = 0;
	    minutes = seconds = hours = 0;

	    id3_main(song, &id3data);
	    strcpy(artist, id3data.artist);
	    strcpy(sname, id3data.songname);
	    scrolla = scroll = 0;
#ifdef LCD
	    LcdClear();
#endif
	    bzero(lcd_bp, 80);
	  }
	  else {
	    strcpy(cmd, "\nclose\n");
	  }
	  break;
	case K_STOP:		/* Stop */
	  strcpy(cmd, "stop\n");
#ifdef DEBUG2
	  printf("State:%d\n", state);
#endif
	  if (state == 3 && plsi) {	/* stopped */
	    /* if stop presset twice, start from the first song */
	    /* Dont reopen the same file twice */
	    track = 0;
	    j = strcmp(song, playlist[0]);
	    if (j) {
	      strcpy(song, playlist[0]);
	      sprintf(cmd, "stop\ninput_open %s\n", song);
	    }
	    id3_main(song, &id3data);
	    strcpy(artist, id3data.artist);
	    strcpy(sname, id3data.songname);
	    scrolla = scroll = 0;
	  }
	  tot_hours = tot_minutes = tot_seconds = 0;
	  elapsed = rem_minutes = rem_seconds = 0;
	  minutes = seconds = hours = 0;
	  scrolla = scroll = 0;
	  offset = 0;
	  break;
	case K_PAUSE:
	  /* pause */
	  if (state == 2) {
	    sprintf(cmd,
		    "seek %d %d\n", offset, range);
	    strcpy(cmd, "play\n");
	  }
	  else
	    strcpy(cmd, "pause\n");
	  break;
	case 'm':
	  /* mute */
	  strcpy(cmd, "output_mute\n");
	  break;
	case K_VOLDOWN:
	  /* volume down */
	  sprintf(cmd,
		  "volume %d %d %d\n",
		  volume -= volume != 0 ? 1 : 0, 75, 50);
	  break;
	case K_VOLUP:
	  /* volume up */
	  sprintf(cmd,
		  "volume %d %d %d\n",
		  volume += volume != 100 ? 1 : 0, 75, 50);
	  break;
	case K_FB:		/* seek left */
	  if (offset > seekoffset)
	    sprintf(cmd,
		    "seek %d %d\n",
		    offset -= seekoffset, range);
	  break;
	case K_FF:		/* seek right */
	  if (offset < range - seekoffset - 1)
	    sprintf(cmd,
		    "seek %d %d\n",
		    offset += seekoffset, range);
	  break;
	case 27:
	  /* arrows */
	  switch (buffer[2]) {	/* arrows */

	  case 'D':		/* left */
	    if (offset > seekoffset)
	      sprintf(cmd,
		      "seek %d %d\n",
		      offset -= seekoffset, range);
	    break;
	  case 'C':		/* right */
	    if (offset < range - seekoffset - 1)
	      sprintf(cmd,
		      "seek %d %d\n",
		      offset += seekoffset, range);
	    break;
	  case 'B':		/* down */
	    sprintf(cmd,
		    "volume %d %d %d\n",
		    volume -= volume != 0 ? 1 : 0, 75, 50);
	    break;
	  case 'A':		/* up */
	    sprintf(cmd,
		    "volume %d %d %d\n",
		    volume += volume != 100 ? 1 : 0, 75, 50);
	    break;
	  };
	  break;
	case 'q':
	case K_QUIT:
#ifdef LCD
	  LcdClear();
#endif
#ifdef USE_SYSLOG
	  closelog();
#endif
#ifdef DEBUG2
	  printf("CMD:exit\n");
#endif
	  slowrite(pfd[1], "exit\n", 5);
	  exit(0);
	  break;
	default:		/* if (isdigit((int)*buffer)) { } else */
	  strcpy(cmd, "");
	}
	/* write(fdc,cmd,strlen(cmd)); *//* Write to console 5 */
	if ((i = strlen(cmd))) {
#ifdef DEBUG2
	  printf("CMD[%02d]:#%s#\n",strlen(cmd),cmd);
#endif
	  slowrite(pfd[1], cmd, i);	/* write to rxaudio */
	}
      }
      /* Input from rxaudio */
      else if (FD_ISSET(pfd2[0], &inout_fds)) {
	char *s;

	response[0] = '\0';
	r = read(pfd2[0], response, 2047);	/* read from rxaudio */
	response[r > 0 ? r : 0] = '\0';
#ifdef DEBUG2
	printf("\nGOT[%d]:#%s#\n", r, response);
#endif
	s = response;
	ack = 0;
	newstate = 0;

	/* parse rxaudio's messages */
	while ((s = strstr(s, "MSG notify "))) {


	  s += 10;
	  sscanf(s, "%s", type);
#ifdef DEBUG3
	  printf("TYPE:%s\n", type);
#endif
	  if (!strcmp(type, "duration")) {
	    s += 11;
	    sscanf(s, "%d", &duration);
	    tot_hours = duration / 3600;
	    tot_minutes = (duration - tot_hours * 3600) / 60;
	    tot_seconds = duration - tot_minutes * 60;
	  }
	  else if (!strcmp(type, "ready")) {
	    ready = 1;
	    /* Initialize rxaudio */
	    sprintf(cmd, "output_open\n");
#ifdef DEBUG2
	    printf("CMD[%02d]:#%s#\n",strlen(cmd),cmd);
#endif
	    slowrite(pfd[1], cmd, strlen(cmd));		/* write to rxaudio */

/*
	    sprintf(cmd, "volume %d %d %d\n", volume, 80, 50);
	    slowrite(pfd[1], cmd, strlen(cmd));		
*/

	    /* close output device on stop/pause/EOF */
	    sprintf(cmd, "set_player_mode 7\n");
#ifdef DEBUG2
	    printf("CMD[%02d]:#%s#\n",strlen(cmd),cmd);
#endif
	    slowrite(pfd[1], cmd, strlen(cmd));		/* write to rxaudio */

	    if (!strlen(plist_file) && strcmp(intro,"none")) {
	      sprintf(cmd, "open %s\nplay\n", intro);
#ifdef DEBUG2
	      printf("CMD[%02d]:#%s#\n",strlen(cmd),cmd);
#endif
	      slowrite(pfd[1], cmd, strlen(cmd));
	    }

	    /*Open the first song from command-line specified playlist*/
	    if (strlen(plist_file)){
	      sprintf(cmd, "input_open %s\n", playlist[0]);
	      tot_hours = tot_minutes = tot_seconds = 0;
	      elapsed = rem_minutes = rem_seconds = 0;
	      minutes = seconds = hours = 0;
	      strcpy(song,playlist[0]);
	      id3_main(song, &id3data);
	      strcpy(artist, id3data.artist);
	      strcpy(sname, id3data.songname);
	      scrolla = scroll = 0;
	      printf("\n****Artist:%s\n",artist);
	      slowrite(pfd[1], cmd, strlen(cmd));
	    }

	    cmd[0] = 0;
	  }
	  else if (!strcmp(type, "ack"))
	    ack = 1;
	  else if (!strcmp(type, "nack"))
	    ack = 0;
	  else if (!strcmp(type, "player")) {
	    sscanf(s = strstr(s, "["), "%s", tmp);
	    if (!strcmp(tmp, "[PLAYING]")) {
	      pstate = state;
	      state = 1;
	      newstate = 1;
	    }
	    else if (!strcmp(tmp, "[PAUSED]")) {
	      pstate = state;
	      state = 2;
	      newstate = 1;
	    }
	    else if (!strcmp(tmp, "[STOPPED]")) {
	      pstate = state;
	      state = 3;
	      newstate = 1;
	    }
	    else if (!strcmp(tmp, "[EOF]")) {
	      pstate = state;
	      state = 4;
	      newstate = 1;
	    }
	  }
	  else if (!strcmp(type, "stream")) {
	    s += 13;
	    sscanf(s, "%s", tmp);
	    sscanf(s = strstr(s, "=") + 1, "%d", &level);
	    sscanf(s = strstr(s, "=") + 1, "%d", &layer);
	    sscanf(s = strstr(s, "=") + 1, "%d", &bitrate);
	    sscanf(s = strstr(s, "=") + 1, "%d", &frequency);
	    sscanf(s = strstr(s, "=") + 1, "%s", mode);
	    mode[strlen(mode) - 1] = '\0';
#ifdef DEBUG3
	    printf("level:%d,layer:%d,br:%d,freq:%d,mode:%s\n",
		   level, layer, bitrate, frequency, mode);
#endif
	  }
	  else if (!strcmp(type, "position")) {
	    sscanf(s = strstr(s, "=") + 1, "%d", &offset);
	    sscanf(s = strstr(s, "=") + 1, "%d", &range);
#ifdef DEBUG3
	    printf("offset:%d,range:%d\n", offset, range);
#endif
	  }
	  else if (!strcmp(type, "timecode")) {
	    sscanf(s = strstr(s, "[") + 1, "%d", &hours);
	    sscanf(s = strstr(s, ":") + 1, "%d", &minutes);
	    sscanf(s = strstr(s, ":") + 1, "%d", &seconds);
#ifdef DEBUG3
	    printf("Hours:%d,Minutes:%d,Seconds:%d\n",
		   hours, minutes, seconds);
#endif
	  }

#ifdef DEBUG3
	  sprintf(tmp, "[%02d:%02d:%02d] (%03d/%03d) Vol:%03d State:%d"
		  " type:%d/%d B/R:%d,freq:%d\n",
		  hours, minutes, seconds, offset, range, volume, state,
		  level, layer, bitrate, frequency);
	  printf("#%s#\n", tmp);
#endif

#ifdef DEBUG3
	  sprintf(tmp, "\r[%02d:%02d:%02d] (%03d/%03d) Vol:%03d State:%d"
	      " type:%d/%d B/R:%d,freq:%d",
		  hours, minutes, seconds, offset, range, volume, state,
		  level, layer, bitrate, frequency);
	  write(fdc, tmp, strlen(tmp));		/* Write to console */
#endif

	  if (plsi) {

	    elapsed = minutes * 60 + seconds;

	    /* Update Main Lcd Screen */
	    lcd_b[0][0] = 0;
	    lcd_bp[0][0] = 0;

#ifdef LCD
	    /* Bar Graph */
	    fprintf(lfp, "%c%c%c%c%c%c",
		    prefix, 0x7c, 0x01, 0x01, 0x00,
	     (unsigned int) ((float) elapsed * (100.0 / (float) duration)));
#else
	    memset(&lcd_b[0][0], ' ', 20);
#endif

	    /* ARTIST */
	    /* Capitalize First Letters, looks nicer that way */
	    for (i = 0; i < strlen(artist); i++) {
	      if (i == 0 && isalpha(artist[i]))
		artist[i] = toupper(artist[i]);
	      else if (artist[i - 1] == ' ' && isalpha(artist[i]))
		artist[i] = toupper(artist[i]);
	    }

	    /* Calculate scroll offset */
	    /* scrolla:scroll offset for artist name */
	    /* scroll:scroll offset for song name */

	    if ((j = strlen(artist)) > 20 && elapsed % scrolldelay) {
	      static unsigned int prevel = 0; /*previous elapsed*/

	      if (scrolla == 0)
		scrda = 0;	/* Go right */
	      else if (scrolla + 20 >= j)
		scrda = 1;	/* Go left */

	      /* scrollstep in chars,scrolldelay in seconds */
	      if (prevel != elapsed && scrda == 0 && scrolla + 20 < j)
		scrolla += scrollstep;
	      else if (prevel != elapsed)
		scrolla -= scrollstep;
	      prevel = elapsed;	/* check every second, not when any message
				 * arrives */
	    }

	    sprintf(tmp, "%-20s", &artist[scrolla]);
	    strncpy(&lcd_b[1][0], tmp, 20);

	    /* SONG NAME */
	    /* Capitalize First Letters */
	    for (i = 0; i < strlen(sname); i++) {
	      if (i == 0 && isalpha(sname[i]))
		sname[i] = toupper(sname[i]);
	      else if (sname[i - 1] == ' ' && isalpha(sname[i]))
		sname[i] = toupper(sname[i]);
	    }


	    /* Calculate scroll offset */
	    if ((j = strlen(sname)) > 20 && elapsed % scrolldelay) {
	      static unsigned int prevel = 0;

	      if (scroll == 0)
		scrd = 0;	/* Go right */
	      else if (scroll + 20 >= j)
		scrd = 1;	/* Go left */

	      if (prevel != elapsed && scrd == 0 && scroll + 20 < j)
		scroll += scrollstep;
	      else if (prevel != elapsed)
		scroll -= scrollstep;

	      prevel = elapsed;
	    }

	    sprintf(tmp, "%-20s", &sname[scroll]);
	    strncpy(&lcd_b[2][0], tmp, 20);

	    rem_minutes = (duration - elapsed) / 60;
	    rem_seconds = (duration - elapsed) % 60;

	    /* Status line, char 126 is the right arrow */
	    sprintf(tmp, "%02d:%02d %c %02d:%02d %-5s   ",
		    rem_minutes, rem_seconds, 126, minutes, seconds,
		    sstate(state));
	    strncpy(&lcd_b[3][0], tmp, 20);



#ifdef LCD
	    LcdUpdate(lcd_bp, lcd_b);
#else
	    WriteScreen(lcd_b);
#endif
	    /* use memcpy/bcopy */
	    for (c = 0; c < 20; c++)
	      for (r = 0; r < 4; r++)
		lcd_bp[r][c] = lcd_b[r][c];
	  }
	  else {
	    strncpy(&lcd_b[0][0], "+------------------+", 20);
	    strncpy(&lcd_b[1][0], "| No File Selected |", 20);
	    strncpy(&lcd_b[2][0], "|Press the Menu Btn|", 20);
	    strncpy(&lcd_b[3][0], "+------------------+", 20);
#ifdef LCD
	    LcdUpdate(lcd_bp, lcd_b);
#else
	    WriteScreen(lcd_b);
#endif
	    for (c = 0; c < 20; c++)
	      for (r = 0; r < 4; r++)
		lcd_bp[r][c] = lcd_b[r][c];
	  }
	  s++;
	}			/* while */

	/* song finished, go to next track */
	/* pstate==1:previous state == play */
	/* state == 4:current state == EOF */
	if (newstate && pstate == 1 && state == 4 && (track < plsi - 1 || repeat)) {
	  if (track < plsi - 1)
	    track++;
	  else			/* repeat */
	    track = 0;

	  /* Don't reopen the same file: rxaudio seems to have a bug in
	   * linux;if you reopen the same file - even after input_close - it
	   * reports file not found(!). This problem doesn't exist at the
	   * solaris version of rxaudio (odd) */
	  j = strcmp(song, playlist[track]);
	  if (j) {
	    strcpy(song, playlist[track]);
	    sprintf(cmd, "stop\ninput_open  %s\n", song);
	  }
	  else
	    strcpy(cmd, "stop\n");

	  strcat(cmd, "play\n");
	  tot_hours = tot_minutes = tot_seconds = 0;
	  elapsed = rem_minutes = rem_seconds = 0;
	  minutes = seconds = hours = 0;
	  id3_main(song, &id3data);
	  strcpy(artist, id3data.artist);
	  strcpy(sname, id3data.songname);
	  scrolla = scroll = 0;

#ifdef DEBUG2
	  printf("CMD:#%s#\n", cmd);
#endif
	  write(pfd[1], cmd, strlen(cmd));	/* write to rxaudio */
	}
	else if (newstate && pstate == 1 && state == 4) {
	  strcpy(cmd, "stop\nseek 0 100\n");
#ifdef DEBUG2
	  printf("CMD:#%s#\n", cmd);
#endif
	  write(pfd[1], cmd, strlen(cmd));	/* write to rxaudio */
	}

      }				/* read from rxaudio */

    } while (1);		/*while (i!=EINTR)*/
    my_error("main","select", errno);
  }				/* else if parent */

  return errno;
}				/* main */
