#
#  Makefile for rxcontrol,(c)1998-1999 Spiros Ioannou (sivann@softlab.ntua.gr)
#
#  READ THE README
#
#  DFLAGS' meaning:
#  LCD : define this one to work with a Matrix-Orbital LCD instead of stdout
#  CHVT: define this one to change to a virtual console (under linux) so it
#        can start from init. Virtual console used shouldn't be running a getty.
#        or X. Use this when:
#        a) you don't run X or getty at 'VT' (see rxcontrol.conf)
#        b) a) && you start rxcontrol from /etc/inittab or by hand from a tty
#           
#  USE_SYSLOG: log warning and error messages to syslog (user level)
#  DEBUG0: debug messages - basic rxcontrol operation
#  DEBUG1: debug messages - internal rxcontrol operation
#  DEBUG2: debug messages - protocol/rxaudio information
#  DEBUG3: debug messages - protocol/rxaudio decoded information
#
# Examples:
# Run from an xterm and have an LCD connected:
# Note: Arrow keys won't work correctly with X, use alternate keys
# DFLAGS = -DLCD 
#
# run without X and get keybord input from virtual console `VT'
# DFLAGS = -DLCD  -DCHVT


#Debugging FLAGS:
CFLAGS = -g -Wmissing-prototypes -Wall 

#Release FLAGS:
#CFLAGS = -O3 -s -Wmissing-prototypes -Wall

#comment out next line to disable Infra Red support (LIRC)
#uncomment it to support IR
#HAVEIR = y



#used in mp3-o-phono:
#DFLAGS = -DLCD -DCHVT -DUSE_SYSLOG

#Debugging info flags:
#DFLAGS = -DDEBUG0 -DDEBUG1 


################################################################
##### YOU DON'T NEED TO CHANGE ANYTHING BELOW THIS LINE ########
################################################################


OBJECT = lcd.o rxcontrol.o id3.o fstab.o readconf.o kbrate.o
IROBJECT = ir.o ir_loadkeys.o
IRDEP=


ifdef HAVEIR
DFLAGS+=-DHAVEIR
OBJECT+=$(IROBJECT)
IRDEP=ir.h
endif



all: rxcontrol pipefilter
#all: 
#	@echo Objects: $(OBJECT)
#	@echo Dflags: $(DFLAGS)
#	@echo " "
#
rxcontrol : $(OBJECT)  
	gcc $(CFLAGS) -o rxcontrol $(OBJECT) 
		

pipefilter : pipefilter.c
	gcc $(CFLAGS) $(DFLAGS) pipefilter.c -o pipefilter

rxcontrol.o : rxcontrol.c id3.h rxcontrol.h rxkeys.h readconf.h\
              $(IRDEP) Makefile rxcontrol.conf
	gcc $(CFLAGS) $(DFLAGS) -c rxcontrol.c 

readconf.o: readconf.c readconf.h
	gcc $(CFLAGS) $(DFLAGS) -c readconf.c 

id3.o: id3.c id3.h 
	gcc $(CFLAGS) $(DFLAGS) -c id3.c 

lcd.o: lcd.c  
	gcc $(CFLAGS) $(DFLAGS) -c lcd.c 

kbrate.o: kbrate.c 
	gcc $(CFLAGS)  -c kbrate.c 

fstab.o: fstab.c 
	gcc $(CFLAGS)  -c fstab.c 

ir.h:  ir_loadkeys.h

ir.o:	ir.c ir.h ir_loadkeys.o
	gcc $(CFLAGS) -c ir.c 

ir_loadkeys.o:	ir.h rxkeys.h ir_loadkeys.c ir_loadkeys.h
	gcc $(CFLAGS) -c ir_loadkeys.c 

ir_loadkeys.h:	irkeys.conf
	@echo generating ir_loadkeys.c from irkeys.conf
	./conf2c

ir_loadkeys.c:	irkeys.conf
	@echo generating ir_loadkeys.c from irkeys.conf
	./conf2c

clean:  
	rm -f $(OBJECT) rxcontrol core ir_loadkeys.c ir_loadkeys.h pipefilter
