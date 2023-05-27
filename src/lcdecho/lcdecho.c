/* Spiros Ioannou 1998 */

#include <stdio.h>
#include <string.h>
#include <termio.h>
#include <ioctls.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

main(int argc,char **argv)
{
int fd,r,i,j,k;
unsigned char c,*s;
unsigned char uc;
unsigned char buff[1024];
unsigned char cmd[256];
FILE *lfp;

struct termios * termios_p;

    if (argc != 3) {
      fprintf(stderr,"Usage:%s <port> <message>\n",argv[0]);
      fprintf(stderr,"\nWrites <message> to lcd in <port> at 19200bps\n\n");
      exit(-1);
    }

    if((lfp = fopen(argv[1],"w")) == 0) {
        perror(argv[1]);
        exit(1);
    }

    setvbuf(lfp,(char *)0,_IONBF,0); /*Unbuffered output to LCD*/
    setvbuf(stdout,(char *)0,_IONBF,0); /*And to stdout*/

    /*Set Line Parmeters*/
    termios_p=(struct termios *)malloc(sizeof(struct termios));
    tcgetattr(fileno(lfp),termios_p);
    cfsetospeed(termios_p,B19200);
/*    cfmakeraw(termios_p);  /*No translations!*/
    tcsetattr(fileno(lfp),TCSANOW,termios_p);



    /*Configure LCD*/
    fprintf(lfp,"%cC",0xFE); /*Wrapping On*/
    fprintf(lfp,"%cQ",0xFE); /*Scroll On*/
    fprintf(lfp,"%cJ",0xFE); /*Cursor On*/
    fprintf(lfp,"%c",12); /*clear*/


    fprintf(lfp,"%s",argv[2]);

}

