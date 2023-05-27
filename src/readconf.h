#include <stdio.h>

typedef struct {
  char soption[128]; /* a string option (req)    */
  char svalue[128];  /* a string value (ret)     */
  int value;         /* an integer value (ret)   */
  int type;          /* the type (int or string) */
  int error;         /* the error                */
} conf_s;


int readconf(FILE *,conf_s *);
