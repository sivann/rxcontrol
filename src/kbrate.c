/* kbrate.c  Spiros Ioannou 1999 */
/* change keyboard rate+delay    */
/* Based on kbdrate by Rickard E. Faith (faith@cs.unc.edu)*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include "rxcontrol.h" 		/* needed for my_warn() */


static int valid_rates[] = { 300, 267, 240, 218, 200, 185, 171, 160, 150,
                                   133, 120, 109, 100, 92, 86, 80, 75, 67,
                                   60, 55, 50, 46, 43, 40, 37, 33, 30, 27,
                                   25, 23, 21, 20 };

static int valid_delays[] = { 250, 500, 750, 1000 };

#define RATE_COUNT  (sizeof( valid_rates )  / sizeof( int ))
#define DELAY_COUNT (sizeof( valid_delays ) / sizeof( int ))

int kbrate(int kbrate,int delay)
{
   double      rate;
   int         value = 0x7f;    /* Maximum delay with slowest rate */
                                /* DO NOT CHANGE this value */
   int         fd;
   unsigned char   data;
   int         i;
   extern char *optarg;

if (!kbrate)rate = 10.9;     /* Default rate */
if (!delay) delay = 250;     /* Default delay */
rate=(double)kbrate;

  for (i = 0; i < RATE_COUNT; i++)
    if (rate * 10 >= valid_rates[i]) {
      value &= 0x60;
      value |= i;
      break;
    }


    for (i = 0; i < DELAY_COUNT; i++)
      if (delay <= valid_delays[i]) {
	value &= 0x1f;
	value |= i << 5;
	break;
      }

    if ( (fd = open( "/dev/port", O_RDWR )) < 0) {
      my_warn("kbrate","/dev/port",errno);
      return 0;
    }

    do {
      lseek( fd, 0x64, 0 );
      read( fd, &data, 1 );
    } while ((data & 2) == 2 );  /* wait */

    lseek( fd, 0x60, 0 );
    data = 0xf3;                 
    write( fd, &data, 1 );

    do {
      lseek( fd, 0x64, 0 );
      read( fd, &data, 1 );
    } while ((data & 2) == 2 );  /* wait */

    lseek( fd, 0x60, 0 );
    sleep( 1 );
    write( fd, &value, 1 );

    close( fd );

#ifdef DEBUG1
    if (!silent)printf( _("Typematic Rate set to %.1f cps (delay = %d mS)\n"),
			 valid_rates[value & 0x1f] / 10.0,
			 valid_delays[ (value & 0x60) >> 5 ] );
#endif

 return 0;
}
