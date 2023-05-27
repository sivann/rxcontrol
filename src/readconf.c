/* Read rxcontrol's configuration file.
 * needs rewrite from scratch
 */

#include <stdio.h>
#include <string.h>
#include "readconf.h"



int
readconf(FILE *confp,conf_s * cs)
{
  char line[256];
  char opt[128];
  char sval[128];
  int val=-1;
  int linenum=0;
  
  cs->value=0;
  cs->svalue[0]=0;

  rewind(confp);

  while(fgets(line,256,confp)) {
    opt[0]=sval[0]=0;
    linenum++;
    if (*line==' ' || *line=='#' || *line=='\n' || *line=='\t') 
      continue;

    if (cs->type==0) { /*string*/
      sscanf(line,"%s%s",opt,sval);
      if (!strlen(sval)) {
	fprintf(stderr,"readconf:Error in configuration file,line %d\n",
	    linenum);
	fprintf(stderr,"        :%s=%s\n\n",opt,sval);
	cs->error++;
      }
      if (!strcmp(opt,cs->soption)) {
	strcpy(cs->svalue,sval);
	return 1;
      }
    }
    else if (cs->type==1) { /*int*/
      sscanf(line,"%s%d",opt,&val);
      if (strcmp(opt,cs->soption)) 
	continue;
      if (val==-1) {
	fprintf(stderr,"readconf:Error in configuration file,line %d\n",
	    linenum);
	fprintf(stderr,"        :%s==%d\n\n",opt,val);
	cs->error++;
      }
      if (!strcmp(opt,cs->soption)) {
	cs->value=val;
	return 1;
      }
    }

  }
  fprintf(stderr,"\nreadconf:Error:configuration file missing option:%s\n\n",
      cs->soption);
  cs->error++;
return 0;
}
