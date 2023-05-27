/*SiVaNn's LCD comm routines*/
/*For the Matrix-Orbital 20x4 LCD2041 module*/
/*They really do work!*/
/*E-mail sioan@cc.ece.ntua.gr */

/*Version 1d */


#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <sgtty.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "lcd.h"

int     c=-1;
int  	prefix=0xFE;

extern FILE * lfp;

struct termios * termios_p;


/* Some sweep-like update effects for the lcd
 * Usage: LcdSweep(table20x4 shown now by the lcd,new table to be shown,mode);
 */
void LcdSweep(unsigned char (*lcd_p)[20],unsigned char (*lcd_n)[20],int mode)
{
  time_t delay=80000; /*usecs*/
  unsigned char lcd_h[4][20];
  unsigned char lcd_hp[4][20];
  int r,c,j,max;

  switch (mode) {
  case 0: /*Horizontal , left to right*/

    memcpy(lcd_h,lcd_p,80);
    memcpy(lcd_hp,lcd_p,80);

    for (c=0;c<20;c++) {
      for (r=0;r<4;r++) {
	if (c) lcd_h[r][c-1]=lcd_n[r][c-1];
        lcd_h[r][c]=255;
      }
    LcdUpdate(lcd_hp,lcd_h);
    memcpy(lcd_hp,lcd_h,80);
    if (c!=19) usleep(delay);
    }
    LcdUpdate(lcd_h,lcd_n);

    break;
  
  case 1: /*Horizontal , right to left */

    memcpy(lcd_h,lcd_p,80);
    memcpy(lcd_hp,lcd_p,80);

    for (c=19;c>=0;c--) {
      for (r=0;r<4;r++) {
	if (c<19) lcd_h[r][c+1]=lcd_n[r][c+1];
        lcd_h[r][c]=255;
      }
    LcdUpdate(lcd_hp,lcd_h);
    memcpy(lcd_hp,lcd_h,80);
    usleep(delay);
    }
    LcdUpdate(lcd_h,lcd_n);

    break;

  case 2: /*Vertical , top to bottom*/

    memcpy(lcd_h,lcd_p,80);
    memcpy(lcd_hp,lcd_p,80);

    for (r=0;r<4;r++) {
      for (c=0;c<20;c++) {
        lcd_h[r][c]=255;
        if (r) lcd_h[r-1][c]=lcd_n[r-1][c];
      }
      LcdUpdate(lcd_hp,lcd_h);
      memcpy(lcd_hp,lcd_h,80);
      usleep(delay);
    }
    LcdUpdate(lcd_h,lcd_n);

    break;

  case 3: /*Vertical , bottom to top */

    memcpy(lcd_h,lcd_p,80);
    memcpy(lcd_hp,lcd_p,80);

    for (r=3;r>=0;r--) {
      for (c=0;c<20;c++) {
        lcd_h[r][c]=255;
        if (r<3) lcd_h[r+1][c]=lcd_n[r+1][c];
      }
      LcdUpdate(lcd_hp,lcd_h);
      memcpy(lcd_hp,lcd_h,80);
      usleep(delay);
    }
    LcdUpdate(lcd_h,lcd_n);
    break;

  case 4: /*pseudo-random character replacement
           *change 33 to something else for other
	   *lcd sizes*/

    max=0;
    j=0;
    memcpy(lcd_h,lcd_p,80);
    memcpy(lcd_hp,lcd_p,80);
    do {
      max++;
      lcd_h[j/20][j%20]=lcd_n[j/20][j%20];
      LcdUpdate(lcd_hp,lcd_h);
      memcpy(lcd_hp,lcd_h,80);
      usleep(delay/10);
      if (max<=0) LcdUpdate(lcd_h,lcd_n);
      j=(j+33)%80;
    }
    while (max<=80);
    break;

  }

}



void LcdClear()
{
 fprintf(lfp,"%c",0x0C);
}

void LcdWrite2(unsigned char  (*lcd)[20])
{
    int r=0,c=0;

    for (r=0;r<4;r++){
      if (lcd[r][0]) {
       fprintf(lfp,"%cG%c%c",prefix,1,r+1);
       for (c=0;c<20;c++)
         fprintf(lfp,"%c",lcd[r][c]);
 
      }
    }
}



/* Write an 20*4 char table to LCD*/
void LcdWrite(unsigned char  (*lcd)[20])
{
    int r,c;

    LcdClear();
    for (r=0;r<4;r++)
      for (c=0;c<20;c++)
        fprintf(lfp,"%c",lcd[r][c]);
}



/* Write an 20*4 char table to LCD but only output the changes
 * from the previous screen, thus speeding up the process a LOT
 * and eliminating the annoying flickering/flashing.
 * Usage: LcdUpdate(table20x4 shown now by the lcd,new table to be shown);
 *
 * example:
 *  unsigned char lcd_before[4][20];
 *  unsigned char lcd_after[4][20];
 *  strncpy(&lcd_before[0][0],"********************",20);
 *  strncpy(&lcd_before[1][0],"*    LALALLALA     *",20);
 *  strncpy(&lcd_before[2][0],"*   Screen one     *",20);
 *  strncpy(&lcd_before[3][0],"********************",20);
 *
 *  strncpy(&lcd_after[0][0],"********************",20);
 *  strncpy(&lcd_after[1][0],"*    Trala       ro*",20);
 *  strncpy(&lcd_after[2][0],"*   Another screen *",20);
 *  strncpy(&lcd_after[3][0],"********************",20);
 *  LcdUpdate(lcd_before,lcd_after);
 *
 */
void LcdUpdate(unsigned char (*lcd_p)[20],unsigned char (*lcd_n)[20])
{

    int r,c,interval,totbytes,zeros,prev=0;

    interval=1;
    fprintf(lfp,"%cH",prefix); /*Go to Top Left*/
    for (r=0;r<4;r++) {
      totbytes=zeros=0;

      /* Count Total bytes to be sent in this line *
       * including lcd positioning commands        */
      for (c=0;c<20;c++)
          if(lcd_n[r][c]==lcd_p[r][c]) {
	    zeros++;
	    prev=0;
	  }
	  else {
	    if (c>0 && prev==0) 
	      totbytes+=4;
	    prev=1;
	  }
      totbytes+=20-zeros;

      if (totbytes>20) { /*Too many, just send the whole line*/
        fprintf(lfp,"%cG%c%c",prefix,1,r+1);/*go to column#1 of this row*/
        for (c=0;c<20;c++){
	  fprintf(lfp,"%c",lcd_n[r][c]);
	}
      }
      else { /*don't copy the whole line, use the G comand*/
        for (c=0;c<20;c++) {
          if(lcd_n[r][c]-lcd_p[r][c]) {
	    if (interval) {
	      fprintf(lfp,"%cG%c%c",prefix,c+1,r+1);
	      fprintf(lfp,"%c",lcd_n[r][c]);
	      interval=0;
	    }
	    else /*if(lcd_n[r][0])*/ { /*interval=0*/
	      fprintf(lfp,"%c",lcd_n[r][c]);
	    }
          }
	  else 
	    interval=1;
	}/*for n-p*/
      } 
    } /*for r*/
}

/*Create custom greek characters not included in LCD's char table*/
/*Capital Letters*/
void LcdInitGreek()
{
/* User font 0 left unused so that a string can be easily printed 
 * with standard C string functions like fputs,fprintf,e.t.c.
 * letters not defined here, are already in tha character font chart
 */

    /*Delta pronounced like "th" in "the"*/
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,1,4,10,17,17,17,17,31,0);

    /*Delta (alternate)*/
    /*
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,1,4,10,10,17,17,17,31,0);
    */

    /*Lambda pronounced "L"*/
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,2,4,10,17,17,17,17,17,0);

    /*Ksi pronounced "ks" like "x" in "axe"*/
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,3,31,0,0,14,0,0,31,0);

    /*Pi pronounced "P" like in "portal"*/
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,4,31,17,17,17,17,17,17,0);

    /*Fi pronounced "f" like in "fish"*/
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,5,14,21,21,14,4,4,4,0);

    /*Psi pronounced "ps" like in "epson"*/
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,6,21,21,21,14,4,4,4,0);

    /*Gamma pronounced like w in "what" or like y in "yes"*/
    fprintf(lfp,"%cN%c%c%c%c%c%c%c%c%c",prefix,7,31,16,16,16,16,16,16,0);
}



/* Takes as argument a greek string in 928 code page, and returns
 * the same string in LCD's charset, so greek text in 928 can be
 * displayed correctly to the LCD. LcdInitGreek must be called
 * before though,for some letters to initialize
 * ex: fprintf(lfp,"%s",LcdTogreek("áâãäåæçèéêëìíîïðñóôõö÷øù"));
 * (Must have greek fonts (eg. elot14.pcf) to see the above correctly ;-)
 * codepage 928 is iso8859-7
 */
char * LcdTogreek(char * str)
{ 
unsigned char c,tc;
static char * tstr;
int i;

tstr=(char *)realloc(tstr,strlen(str)+1);

  for(i=0;i<strlen(str);i++) {
      c=str[i];
      if (225<=c && c<=249)  c-=32; /*turn lowercase to uppercase (tolower)*/
      switch (c) {
	case 220 : 	tc='A'; /*a accent*/
			break;
	case 193 : 	tc='A'; /*Alpha*/
			break;
	case 194 : 	tc='B'; /*Beta*/
			break;
	case 195 : 	tc=7;
			break;
	case 196 : 	tc=1;
			break;
	case 221 : 	tc='E'; /*e accent*/
			break;
	case 197 : 	tc='E';
			break;
	case 198 : 	tc='Z';
			break;
	case 199 : 	tc='H';
			break;
	case 200 : 	tc=242; /*theta*/
			break;
	case 250 : 	tc='I'; /*iota dialutika*/
			break;
	case 223 : 	tc='I'; /*iota accent */
			break;
	case 201 : 	tc='I';
			break;
	case 202 : 	tc='K';
			break;
	case 203 : 	tc=2;
			break;
	case 204 : 	tc='M';
			break;
	case 205 : 	tc='N';
			break;
	case 206 : 	tc=3;
			break;
	case 252 : 	tc='O'; /*o accent*/
			break;
	case 207 : 	tc='O';
			break;
	case 208 : 	tc=4;
			break;
	case 209 : 	tc='P';
			break;
	case 211 : 	tc=246; /*Sigma*/
			break;
	case 212 : 	tc='T';
			break;
	case 253 : 	tc='Y'; /*upsilon accent*/
			break;
	case 213 : 	tc='Y';
			break;
	case 214 : 	tc=5;
			break;
	case 215 : 	tc='X';
			break;
	case 216 : 	tc=6;
			break;
	case 217 : 	tc=244; /*Omega*/
			break;
	default	 :	tc=c;
      }
    tstr[i]=tc;
    }
return tstr;
}

