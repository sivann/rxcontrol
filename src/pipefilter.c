/*pipefilter.c*/
/*this is called by rxcontrol*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <termio.h>


char rxpath[128];

char buffer[2048];
int  slowrite(int fd, char *buf, int count);
void no_wait(int fd);

void
no_wait(int fd)
{				/* Accept key input a.s.a.p.:don't wait for * 
				 * Enter */
  struct termio ts;

  ioctl(fd, TCGETA, &ts);
  ts.c_cc[VMIN] = 1;
  ts.c_lflag = ISIG;
  ioctl(fd, TCSETA, &ts);
}

int
main(int argc,char **argv)
{
  int  pfd_out[2], pfd_in[2], fdc;
  int  childpid, r,w, i;
  fd_set inout_fds;
  struct timeval tval;
  char c;
  int output=1;

#ifdef DEBUG2
  FILE *fp;

  fp=fopen("/tmp/pipfilter-debug","w");
  fflush(fp);
#endif
  pipe(pfd_out);		/* write to  exec'ed process (rxaudio) */
  pipe(pfd_in);			/* read from exec'ed process (rxaudio) */

  if (argc==1) {
    printf("You are not supposed to run this\n");
    exit(1);
  }

  childpid = fork();

  if (childpid == 0) {

    /* close unused side of pipes */
    close(pfd_out[1]);
    close(pfd_in[0]);

    close(0);
    dup(pfd_out[0]);		/* read from here */
    close(1);
    dup(pfd_in[1]);		/* write here */

    strcpy(rxpath,argv[1]);
    //fprintf(stderr,"*** rxpath:      %s\n",rxpath);

    r = execlp(rxpath, "rxaudio", NULL);

    perror("execlp:rxaudio");
    kill(childpid, 15);
    exit(0);
  }

  fdc=0;
  no_wait(fdc);

  /* close unused side of pipes */
  close(pfd_out[0]);
  close(pfd_in[1]);

  do {
    FD_ZERO(&inout_fds);
    FD_SET(pfd_in[0], &inout_fds);	/* FROM rxaudio */
    FD_SET(fdc, &inout_fds);		/* FROM stdin */
    tval.tv_sec = 1;			/* Seconds */
    tval.tv_usec = 0;			/* microseconds */

    /* No timeout (interrupt driven) select */
    i = select(16, &inout_fds, (fd_set *) 0, (fd_set *) 0,(struct timeval*)0 );

    /* Input from stdin */
    if (FD_ISSET(fdc, &inout_fds)) {
      r = read(fdc, &c, 1);
      //printf("%c",c);
      //fflush(stdout);
      if (c==24)
	output=1;
      if (c==16)
	output=0;
      /* write to rxaudio */
      if (c!=24 && c!=16){
	slowrite(pfd_out[1], &c, 1);
#ifdef DEBUG2
	fprintf(fp,"->rxaudio:%c[%d]\n",c,c);
	fflush(fp);
#endif
      }
    }
    /* Input from rxaudio */
    if (FD_ISSET(pfd_in[0], &inout_fds)) {

      bzero(buffer,sizeof(buffer));
      r = read(pfd_in[0], buffer, 2047);	/* read from rxaudio */
      if (output) {
	w = write(1, buffer, r);		/* write to stdout */
#ifdef DEBUG2
	fprintf(fp,"rxaudio sais:%s\n",buffer);
	fflush(fp);
#endif
      }
      if (!r)
	exit(2);
    }
  } while (1);			/* while (i!=EINTR) */
  return 1;
}

int
slowrite(int fd, char *buf, int count)
{
  int  r = 0;

  while (count--) {
    if (write(fd, buf, 1))
      r++;
    if (*buf == '\n')
      usleep(5000);
    buf++;
  }
  return r;
}
