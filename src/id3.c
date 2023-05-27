/* MPEG-Audio ID3 TAG read*/
/* Credits to Woo-jae Jung for the original TAG code fragments*/

/* Spiros Ioannou 1998*/
/* Added: some comments & capability
 * of guessing artist/song from filename
 * if ID3TAG doesn't exist*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "id3.h"

#include <ctype.h>


void removespace(char *str,int max);
void fillspace(char *str,int max);
void parseID3(char *filename,ID3 *data,int flag);
void readID3(FILE *fp,ID3 *data);
int id3_main(char *filename,ID3 *data);

void removespace(char *str,int max)
{
  int i;

  str[max]=0;

  for(i=max-1;i>=0;i--)
    if(((unsigned char)str[i])<26 || str[i]==' ')str[i]=0; else break;
}

void fillspace(char *str,int max)
{
  int i;

  for(i=0;i<max;i++)
    if(str[i]==0)break;
  for(;i<max;i++)str[i]=' ';
}

void parseID3(char *filename,ID3 *data,int flag)
{
char *p,*q,*r,*sl,*t;
char fn[256];
char buff[256];
int i,j,k,m;

bzero(fn,256);
memcpy(fn,filename,strlen(filename));

    if (flag) {

      /*
      printf("\n"
	 "Filename: %s\n"
	 "\t1(Song)   :%s\n"
	 "\t2(Artist) :%s\n"
	 "\t3(Album)  :%s\n"
	 "\t4(Year)   :%s\n"
	 "\t5(Comment):%s\n"
	 "\t6(Type)   :%d\n",
	 filename,
	 data->songname,
	 data->artist,
	 data->album,
	 data->year,
	 data->comment,
	 data->type);
	 */
    }
    else {
     p=q=r=0;
     m=0;
     /*
     printf("guessing from filename...\n\n");
     */

     sl=strrchr(fn,'/');
     if (sl) sl++; else sl=fn;
     strcpy(buff,sl);

     p=strtok(sl,"-"); 			/*Artist*/
     while(p && *p==' '){p++;m++;}
     if ((t=strstr(p,".mp3"))) 
       {*t=0;m+=4;};

     if (p) {
       q=strtok(0,"-"); 		/*Songname*/
       while(q && *q==' '){q++;m++;}
       if (q) { 
	 if ((t=strstr(q,".mp3"))) 
	   {*t=0;m+=4;}
	 r=buff+strlen(q)+strlen(p)+m+1;	/*the rest*/
         while(r && *r && !isalnum(*r))r++;
	 if ((t=strstr(r,".mp3"))) *t=0;
       }
     }
     /*Cut off the trailling spaces*/
     i=j=k=0;
     if (p) for (i=strlen(p)-1;i>=0 && p[i]==' ';i--) p[i]=0;
     if (q) for (j=strlen(q)-1;j>=0 && q[j]==' ';j--) q[j]=0;
     if (r) for (k=strlen(r)-1;k>=0 && r[k]==' ';i--) r[k]=0;

     /*
     printf("\tArtist:%s\n\tSong:%s\n\tOther:%s\n\n",p,q,r);
     */
     if (p) strncpy(data->artist,p,30);
     if (q) strncpy(data->songname,q,30);
     if (r) strncpy(data->comment,r,30);

    }
}

void readID3(FILE *fp,ID3 *data)
{
  fread(data->songname,30,1,fp);removespace(data->songname,30);
  fread(data->artist  ,30,1,fp);removespace(data->artist  ,30);
  fread(data->album   ,30,1,fp);removespace(data->album   ,30);
  fread(data->year    , 4,1,fp);removespace(data->year    , 4);
  fread(data->comment ,30,1,fp);removespace(data->comment ,30);
  fread(&(data->type) , 1,1,fp);
}

int id3_main(char *filename,ID3 *data)
{
  FILE *fp;
  int offset,flag=0;

  bzero(data,sizeof(ID3));

  if (!(fp=fopen(filename,"r")))
  { char str[128];
    sprintf(str,"id3_main:%s",filename);
    perror(str);
    return -1;
  }

  fseek(fp,-128,SEEK_END);

  offset=flag=0;
  if(getc(fp)==0x54)     /*T*/
    if(getc(fp)==0x41)   /*A*/
      if(getc(fp)==0x47) /*G*/
      {
	offset=ftell(fp)-3;
	readID3(fp,data);
	flag=1;
      }

  if(!flag)             
  {
    fseek(fp,-125,SEEK_END);

    offset=flag=0;
    if(getc(fp)==0x54)
      if(getc(fp)==0x41)
	if(getc(fp)==0x47)
        {
	  offset=ftell(fp)-3;
	  readID3(fp,data);
	  flag=1;
	}
  }

  fclose(fp);
  parseID3(filename,data,flag);
  return flag;
}
