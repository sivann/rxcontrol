void LcdUpdate(unsigned char(*)[20] ,unsigned char(*)[20] );
void LcdSweep(unsigned char (*lcd_p)[20],unsigned char (*lcd_n)[20],int mode);
void LcdClear(void);
void LcdWrite2(unsigned char  (*lcd)[20]);
void LcdWrite(unsigned char  (*lcd)[20]);
void LcdInitGreek(void);
char * LcdTogreek(char * str);
