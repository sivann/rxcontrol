/* Spiros Ioannou 1998 */
#include <stdio.h>
#include <string.h>
#include <termio.h>
#include <ioctls.h>
#include <linux/vt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



/*SVR4 method*/
no_wait(int fd)
{

    int i;
    struct termio ts;


    ioctl(fd, TCGETA, &ts);

    ts.c_cc[VMIN]=1;
    ts.c_lflag = ISIG;

    ioctl(fd, TCSETA, &ts);
}

chvt(int vt)
{
int ctfd;

    ctfd=open("/dev/tty", O_RDONLY) ;
    ioctl(ctfd, VT_ACTIVATE, vt) ;
    ioctl(ctfd, VT_WAITACTIVE,vt);
    close(ctfd);

}


main()
{
int fd,r,i,j,k;
unsigned char c,*s;
unsigned char uc;
int pid;
unsigned char buff[1024];
unsigned char cmd[256];
FILE *lfp,*sfp;

struct termios * termios_p;


    if((lfp = fopen("/dev/cua1", "w")) == 0) {
        perror("fopen:cua1");
        exit(1);
    }

    setvbuf(lfp,(char *)0,_IONBF,0); /*Unbuffered output to LCD*/
    setvbuf(stdout,(char *)0,_IONBF,0); /*And to stdout*/

    /*Set Line Parmeters*/
    termios_p=(struct termios *)malloc(sizeof(struct termios));
    tcgetattr(fileno(lfp),termios_p);
    cfsetospeed(termios_p,B19200);
    cfmakeraw(termios_p);  /*No translations!*/
    tcsetattr(fileno(lfp),TCSANOW,termios_p);



    /*Configure LCD*/
    fprintf(lfp,"%cC",0xFE); /*Wrapping On*/
    fprintf(lfp,"%cQ",0xFE); /*Scroll On*/
    fprintf(lfp,"%cJ",0xFE); /*Cursor On*/
    fprintf(lfp,"%c",12); /*clear*/



    sfp=popen("/bin/tcsh -f 2>&1","r");
    sprintf(cmd,"%cG",0xFE);


    while(r>=0){
	r=read(fileno(sfp),buff,256);
	buff[r]=0;
	s=strstr(buff,cmd);
	if (s) {
	  printf("\n\r[%d]bef:%d %c %d %d %d %d\n",r,s[0],s[1],s[2],s[3],s[4],s[5]);
	  if (s[2]==13 && s[3]==10) {s[2]=s[3];s[3]=s[4];s[4]=0;}
/*	    for (i=2;i<r-1;i++) s[i]=s[i+1];*/
	  printf("\r Saft:%d %c %d %d %d\n",s[0],s[1],s[2],s[3],s[4]);
	  
	}

	write(fileno(lfp),buff,r);
    }

}

